// pages/detail/detail.js
// M1 实验详情页
// 展示：综合评分大数字 + 四维雷达图（Canvas 2D 五边形：四维 + 综合）+ 四维数值 + 布料名/时间
// 约定（§0.3 / §3.1）：
//   · softness/smoothness/roughness/rebound 为 STM32 归一化后的 [0,1]（metricsNormalized=true），展示 ×100
//   · composite 为 0-100，权威值来自 STM32 FRAME_SCORE，前端只显示、不重算
// 说明：刻意使用 Promise 链而非 async/await，兼容较低基础库 / 未开增强编译的情况

const cloudApi = require("../../utils/cloud.js");
const fmt = require("../../utils/fmt.js");

Page({
  data: {
    id: "",
    loading: true,
    errMsg: "",
    record: null,       // 原始记录（字段白名单裁剪后）
    compositeText: "--",
    fabricName: "未命名布料",
    deviceId: "",
    createdAtText: "--",
    startedAtText: "--",
    dedupId: "",
    source: "",
    metricsNormalized: true,
    // 四维展示值（0-100 整数；细腻度已取反：值越大越细腻）
    metrics: [],
  },

  /** Canvas 2D 上下文与尺寸（放在 data 外，避免无谓 setData） */
  _ctx: null,
  _cssSize: 0,

  onLoad(options) {
    const id = (options && options.id) || "";
    this.setData({ id: id });
    if (!id) {
      this.setData({ loading: false, errMsg: "缺少记录 id，无法加载详情" });
      return;
    }
    this.loadDetail(id);
  },

  onReady() {
    // 注意：canvas 位于 `wx:elif="{{record}}"` 内，此时 record 还没加载、节点不存在，
    // 因此不在这里初始化；等数据 setData 之后再初始化并绘制（见 applyRecord）。
  },

  /** 读取单条记录（客户端只读，见 utils/cloud.js 说明） */
  loadDetail(id) {
    cloudApi
      .fetchExperimentById(id)
      .then((doc) => {
        if (!doc) {
          this.setData({ loading: false, errMsg: "未找到该记录（可能已被删除）" });
          return;
        }
        this.applyRecord(doc);
      })
      .catch((e) => {
        console.error("[detail] 读取记录失败：", e);
        this.setData({ loading: false, errMsg: cloudApi.describeError(e) });
      });
  },

  /** 把云端文档整理成页面数据，并触发绘制 */
  applyRecord(doc) {
    const normalized = doc.metricsNormalized !== false;
    const softness = fmt.metricToPercent(doc.softness);
    const smoothness = fmt.metricToPercent(doc.smoothness);
    // roughness 越小越细腻 → 展示「细腻度」时取反
    const fineness = 100 - fmt.metricToPercent(doc.roughness);
    const rebound = fmt.metricToPercent(doc.rebound);

    const metrics = [
      { key: "softness", label: "柔软度", desc: "越大越软", value: softness },
      { key: "smoothness", label: "顺滑度", desc: "越大越顺滑", value: smoothness },
      { key: "roughness", label: "细腻度", desc: "越小越细腻（已取反）", value: fineness },
      { key: "rebound", label: "回弹贴合", desc: "越大回弹越好", value: rebound },
    ];

    this.setData(
      {
        loading: false,
        errMsg: "",
        record: doc,
        compositeText: fmt.formatScore(doc.composite),
        fabricName: doc.fabricName || "未命名布料",
        deviceId: doc.deviceId || "",
        createdAtText: fmt.formatTime(doc.createdAt),
        startedAtText: fmt.formatTime(doc.startedAt),
        dedupId: doc.dedupId || "",
        source: doc.source || "",
        metricsNormalized: normalized,
        metrics: metrics,
      },
      // 记录渲染出来（此时 canvas 才存在于 DOM）后再初始化 Canvas 并绘制
      () => wx.nextTick(() => this.initCanvas())
    );
  },

  /** 初始化 Canvas 2D（基础库 2.9.0+ 的新接口） */
  initCanvas() {
    wx.createSelectorQuery()
      .select("#radar")
      .fields({ node: true, size: true })
      .exec((res) => {
        const info = res && res[0];
        if (!info || !info.node) {
          console.warn("[detail] 未取到 canvas 节点，雷达图跳过（不影响数值展示）");
          return;
        }
        const canvas = info.node;
        const ctx = canvas.getContext("2d");
        // 按设备像素比放大，避免高清屏发虚
        const dpr = this.getDpr();
        const size = info.width || 300;
        canvas.width = size * dpr;
        canvas.height = size * dpr;
        ctx.scale(dpr, dpr);

        this._ctx = ctx;
        this._cssSize = size;
        this.drawRadar();
      });
  },

  /** 取设备像素比（新老接口都兼容，取不到时退化为 2） */
  getDpr() {
    try {
      if (wx.getWindowInfo) return wx.getWindowInfo().pixelRatio || 2;
      if (wx.getSystemInfoSync) return wx.getSystemInfoSync().pixelRatio || 2;
    } catch (e) {
      // 忽略：使用默认值
    }
    return 2;
  },

  /**
   * 绘制五边形雷达图
   * 五轴 = 柔软 / 顺滑 / 细腻 / 回弹 / 综合（综合按 0-100 → 0-1 归一）
   * 纯手工坐标计算，不引第三方库（M1 简单实现）
   */
  drawRadar() {
    const ctx = this._ctx;
    if (!ctx) return; // canvas 还没准备好；initCanvas 完成后会再画一次

    const size = this._cssSize || 300;
    const cx = size / 2;
    const cy = size / 2;
    const radius = size / 2 - 46; // 留出轴标签空间
    const metrics = this.data.metrics || [];
    if (!metrics.length) return;

    // 五轴取值（0-1）
    const values = metrics.map((m) => this.clamp01((Number(m.value) || 0) / 100));
    const record = this.data.record || {};
    const compositeNum = Number(record.composite);
    values.push(this.clamp01(isFinite(compositeNum) ? compositeNum / 100 : 0));
    const labels = ["柔软", "顺滑", "细腻", "回弹", "综合"];

    const n = values.length;
    const startAngle = -Math.PI / 2; // 第一轴朝正上方
    function angleOf(i) {
      return startAngle + (i * 2 * Math.PI) / n;
    }
    function pointAt(i, ratio) {
      return {
        x: cx + Math.cos(angleOf(i)) * radius * ratio,
        y: cy + Math.sin(angleOf(i)) * radius * ratio,
      };
    }

    ctx.clearRect(0, 0, size, size);

    // 1) 背景网格（4 圈）
    ctx.lineWidth = 1;
    for (let ring = 4; ring >= 1; ring--) {
      const ratio = ring / 4;
      ctx.beginPath();
      for (let i = 0; i <= n; i++) {
        const p = pointAt(i % n, ratio);
        if (i === 0) ctx.moveTo(p.x, p.y);
        else ctx.lineTo(p.x, p.y);
      }
      ctx.closePath();
      ctx.strokeStyle = ring === 4 ? "#c9d3e0" : "#e6ebf2";
      ctx.stroke();
    }

    // 2) 轴线
    ctx.strokeStyle = "#e6ebf2";
    for (let i = 0; i < n; i++) {
      const p = pointAt(i, 1);
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(p.x, p.y);
      ctx.stroke();
    }

    // 3) 数据多边形
    ctx.beginPath();
    values.forEach((v, i) => {
      const p = pointAt(i, v);
      if (i === 0) ctx.moveTo(p.x, p.y);
      else ctx.lineTo(p.x, p.y);
    });
    ctx.closePath();
    ctx.fillStyle = "rgba(13, 110, 253, 0.22)";
    ctx.fill();
    ctx.lineWidth = 2;
    ctx.strokeStyle = "#0d6efd";
    ctx.stroke();

    // 4) 顶点圆点
    values.forEach((v, i) => {
      const p = pointAt(i, v);
      ctx.beginPath();
      ctx.arc(p.x, p.y, 3, 0, Math.PI * 2);
      ctx.fillStyle = "#0d6efd";
      ctx.fill();
    });

    // 5) 轴标签
    ctx.fillStyle = "#5a6472";
    ctx.font = "12px sans-serif";
    for (let i = 0; i < n; i++) {
      const p = pointAt(i, 1.22);
      ctx.textAlign = Math.abs(p.x - cx) < 2 ? "center" : p.x > cx ? "left" : "right";
      ctx.textBaseline = p.y < cy ? "bottom" : "top";
      ctx.fillText(labels[i], p.x, p.y);
    }
  },

  clamp01(v) {
    const n = Number(v);
    if (!isFinite(n)) return 0;
    if (n < 0) return 0;
    if (n > 1) return 1;
    return n;
  },

  /** 错误态重试 */
  onRetry() {
    if (!this.data.id) return;
    this.setData({ loading: true, errMsg: "" });
    this.loadDetail(this.data.id);
  },
});
