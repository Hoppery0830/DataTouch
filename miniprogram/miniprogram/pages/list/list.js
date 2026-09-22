// pages/list/list.js
// M2 实验记录列表页（首页）：在 M1 的「分页 + 下拉刷新 + 触底加载」基础上补充
//   · 搜索：按 fabricName 模糊匹配（db.RegExp + 300ms 防抖）
//   · 排序：入库时间(createdAt 降序) / 综合评分(composite 降序)
//   · 筛选：数据来源(source) chip + 设备(deviceId) chip（选项来自已加载数据，切换条件后重置到第一页）
//   · 空态 / 错误态 / 加载态文案；顶部显示当前条件与结果条数
//
// 命名约定：四维 softness/smoothness/roughness/rebound 为 STM32 归一化后的 [0,1]，显示时 ×100；
//           细腻度展示时取反 = 100 - roughness×100（规则与 M1/detail 完全一致，不要改）。
// 说明：延续 M1 风格，刻意使用 Promise 链而非 async/await，兼容较低基础库 / 未开增强编译的情况。

const cloudApi = require("../../utils/cloud.js");
const fmt = require("../../utils/fmt.js");

/** 可选的排序档位（value 直接透传给 cloudApi.fetchExperiments 的 orderBy 白名单字段） */
const SORT_OPTIONS = [
  { value: "createdAt", label: "入库时间", desc: true },
  { value: "composite", label: "综合评分", desc: true },
];

/** 数据来源 chip（空 value 表示不筛选） */
const SOURCE_CHIPS = [
  { value: "", label: "全部来源" },
  { value: "device", label: "设备上传" },
  { value: "manual", label: "手工录入" },
];

/** 搜索防抖时长（ms）：输入停止约 300ms 再发起查询 */
const SEARCH_DEBOUNCE_MS = 300;

/** 设备 chip 最多展示几个，避免 chip 行过长 */
const MAX_DEVICE_CHIPS = 8;

Page({
  data: {
    // ---- 列表与分页 ----
    list: [],          // 已加载的记录（已格式化，供 wxml 直接渲染）
    page: 0,           // 已加载页数；下一页页码 = page
    pageSize: cloudApi.PAGE_SIZE || 20,
    hasMore: true,     // 是否还有下一页
    loading: false,    // 是否正在请求（防重入）
    inited: false,     // 是否已完成首次加载
    errMsg: "",        // 错误提示（渲染在页面上，带「重试」）
    sortFailed: false, // 上次失败是否与「非默认排序字段」有关（重试时自动退回入库时间排序）
    envId: "",         // 当前云环境 ID，显示在页头便于确认环境

    // ---- 搜索 / 排序 / 筛选状态 ----
    keyword: "",       // 输入框里的原始文本
    queryKeyword: "",  // 实际生效的搜索词（防抖后 / 回车后）
    sourceValue: "",   // '' = 全部；device / manual
    sourceLabel: "",   // 用于顶部条件摘要
    deviceValue: "",   // '' = 全部设备
    sortValue: "createdAt",

    // ---- 控件选项 ----
    sortOptions: SORT_OPTIONS,
    sourceChips: SOURCE_CHIPS,
    deviceChips: [],   // [{ value, label }]，value 为空表示「全部设备」

    // ---- 展示用派生状态 ----
    activeFilter: false,  // 是否处于筛选态（决定空态文案）
    summaryText: "",      // 顶部：当前条件摘要
    totalText: "",        // 顶部：共 N 条
    sortedHint: "",       // 排序被服务端降级时的提示（一般不用）
  },

  /** 以下为页面私有状态，不放进 data，避免无意义的 setData 开销 */
  _reqSeq: 0,          // 请求序号：丢弃过期响应，避免搜索结果被慢请求覆盖
  _searchTimer: null,  // 搜索防抖计时器
  _deviceCache: [],    // 设备 chip 选项缓存（来自已加载数据，按出现顺序去重）

  onLoad() {
    const app = getApp();
    const envId = (app && app.globalData && app.globalData.env) || "";
    this.setData({ envId });
    // 首次进入加载第一页
    this.loadPage(0, true);
  },

  /** 页面卸载：清掉防抖计时器，避免回调打到已销毁的页面 */
  onUnload() {
    if (this._searchTimer) {
      clearTimeout(this._searchTimer);
      this._searchTimer = null;
    }
  },

  /** 下拉刷新：重置分页后重新加载第一页（保持当前搜索/排序/筛选条件） */
  onPullDownRefresh() {
    this.loadPage(0, true);
  },

  /** 触底：还有更多且不在请求中时加载下一页 */
  onReachBottom() {
    if (this.data.loading || !this.data.hasMore) return;
    this.loadPage(this.data.page, false);
  },

  // ======================= 数据加载 =======================

  /**
   * 加载指定页
   * @param {number} page 从 0 开始的页码
   * @param {boolean} reset 是否重置列表（首屏 / 下拉刷新 / 条件变化 / 重试时为 true）
   */
  loadPage(page, reset) {
    if (this.data.loading) return;

    const seq = ++this._reqSeq; // 本次请求的序号，回来的数据要验证它还"最新"
    const params = this.buildQuery(page);

    this.setData({
      loading: true,
      errMsg: "",
      // 条件变化时先清列表，让用户立刻看到「加载中」而不是旧结果
      list: reset ? [] : this.data.list,
      hasMore: true,
    });

    cloudApi
      .fetchExperiments(params)
      .then((res) => {
        if (seq !== this._reqSeq) return; // 更晚的请求已发出，本次结果作废
        const formatted = (res.list || []).map((item) => this.formatItem(item));
        const list = reset ? formatted : this.data.list.concat(formatted);

        this.patchDeviceChips(formatted);
        this.setData({
          list: list,
          page: page + 1,          // 已加载页数
          hasMore: !!res.hasMore,
          loading: false,
          inited: true,
          sortFailed: false,
        });
        this.updateSummary(res, page, list.length);
      })
      .catch((e) => {
        if (seq !== this._reqSeq) return;
        console.error("[list] 读取 experiments 失败：", e);
        const raw = (e && (e.errMsg || e.message)) || "";
        // 云数据库在「排序字段缺索引」时会直接报错；给出可操作提示，并在重试时自动退回入库时间排序
        const sortFailed =
          this.data.sortValue !== "createdAt" &&
          raw.indexOf("index") >= 0 &&
          raw.indexOf("createdAt") < 0;
        this.setData({
          loading: false,
          inited: true,
          hasMore: false,
          list: reset ? [] : this.data.list,
          sortFailed: sortFailed,
          errMsg: cloudApi.describeError(e),
        });
        this.updateSummary(null, page, reset ? 0 : this.data.list.length);
      })
      .then(() => this.finishPullDown(seq));
  },

  /** 结束下拉刷新动画：只有"最新一次请求"才收尾，避免过期响应提前收起转圈 */
  finishPullDown(seq) {
    if (seq === this._reqSeq) {
      wx.stopPullDownRefresh();
    }
  },

  /**
   * 组装本次查询参数（搜索 / 排序 / 筛选 都收敛到这里，避免各处拼参数）
   * @param {number} page
   * @returns {object} cloudApi.fetchExperiments 的 options
   */
  buildQuery(page) {
    const d = this.data;
    return {
      page: page,
      pageSize: d.pageSize,
      keyword: d.queryKeyword || "",
      source: d.sourceValue || "",
      deviceId: d.deviceValue || "",
      // 当前两档都是降序；用对象形式传给工具函数，便于日后加升序档
      orderBy: { field: d.sortValue || "createdAt", direction: "desc" },
      // 只有第一页才取总数：count() 每次都要额外算一次读
      withTotal: page === 0,
    };
  },

  /**
   * 搜索 / 排序 / 筛选变化：重置到第一页重新查询
   * （注意：必须重置 page，否则 skip 会带着旧页码，出现"第一页是空的"）
   */
  reload() {
    this.setData({ list: [], page: 0, hasMore: true, errMsg: "" });
    if (this.data.loading) {
      // 正在请求中：作废在途请求，等它回来时会被 seq 判断丢弃
      this._reqSeq++;
      this.setData({ loading: false });
    }
    this.loadPage(0, true);
  },

  // ======================= 交互：搜索 =======================

  /** 输入中：只更新输入框回显，等防抖到点再查（避免每敲一个字打一次云数据库） */
  onKeywordInput(e) {
    const value = (e.detail && e.detail.value) || "";
    this.setData({ keyword: value });

    if (this._searchTimer) clearTimeout(this._searchTimer);
    this._searchTimer = setTimeout(() => {
      this._searchTimer = null;
      this.applyKeyword(value.trim());
    }, SEARCH_DEBOUNCE_MS);
  },

  /** 键盘「搜索」：立刻查询，不等防抖 */
  onKeywordConfirm(e) {
    const value = ((e.detail && e.detail.value) || this.data.keyword || "").trim();
    if (this._searchTimer) {
      clearTimeout(this._searchTimer);
      this._searchTimer = null;
    }
    this.applyKeyword(value);
  },

  /** 清空搜索词：立即重置到第一页（不受防抖影响） */
  onClearKeyword() {
    if (this._searchTimer) {
      clearTimeout(this._searchTimer);
      this._searchTimer = null;
    }
    this.setData({ keyword: "" });
    if (!this.data.queryKeyword) return; // 本来就没生效的搜索词，不必重查
    this.applyKeyword("");
  },

  /** 把（防抖后的）搜索词真正生效；与当前生效值相同则不重复请求 */
  applyKeyword(value) {
    const next = value || "";
    const prev = this.data.queryKeyword || "";
    this.setData({ keyword: next, queryKeyword: next });
    if (next === prev) return;
    this.reload();
  },

  // ======================= 交互：排序 / 筛选 =======================

  /** 排序切换（本文档用一排 chip 实现，不用 picker，交互更直接） */
  onSortChange(e) {
    const value = this.readDataset(e, "value");
    if (!value || value === this.data.sortValue) return;
    this.setData({ sortValue: value });
    this.reload();
  },

  /** 筛选 chip：data-type = source | device */
  onFilterTap(e) {
    const type = this.readDataset(e, "type");
    const value = this.readDataset(e, "value");

    if (type === "source") {
      if (value === this.data.sourceValue) return;
      const label = value ? this.sourceLabelOf(value) : "";
      this.setData({
        sourceValue: value,
        sourceLabel: label,
        // 切「全部来源」时同时清掉设备过滤，给用户一个明确的"回到全部"出口
        deviceValue: value ? this.data.deviceValue : "",
      });
      this.reload();
      return;
    }

    if (type === "device") {
      if (value === this.data.deviceValue) return;
      this.setData({ deviceValue: value });
      this.reload();
    }
  },

  /** 顶部「重置」：一键清空搜索 + 筛选，排序保持不动 */
  onResetFilters() {
    if (this._searchTimer) {
      clearTimeout(this._searchTimer);
      this._searchTimer = null;
    }
    this.setData({
      keyword: "",
      queryKeyword: "",
      sourceValue: "",
      sourceLabel: "",
      deviceValue: "",
    });
    this.reload();
  },

  // ======================= 展示与派生状态 =======================

  /**
   * 顶部条件摘要：当前生效的搜索词 / 来源 / 设备，以及结果条数
   * @param {object|null} res 本次请求返回（null 表示失败，不显示总数）
   * @param {number} page 本次页码
   * @param {number} loadedCount 当前已加载条数
   */
  updateSummary(res, page, loadedCount) {
    const d = this.data;
    const parts = [];
    if (d.queryKeyword) parts.push("搜索「" + d.queryKeyword + "」");
    if (d.sourceLabel) parts.push("来源：" + d.sourceLabel);
    if (d.deviceValue) parts.push("设备：" + d.deviceValue);

    const active = parts.length > 0;
    let totalText = "";
    if (res) {
      if (page === 0 && res.total !== null && res.total !== undefined) {
        // 第一页才拿到 total，顺带算出「共 N 条」
        totalText = "共 " + res.total + " 条";
      } else if (loadedCount) {
        totalText = "已加载 " + loadedCount + " 条";
      }
    }

    // sortedBy === "composite-memory" 说明服务端按 composite 排序没拿到数据，
    // 走了「取一页 + 内存排序」的兼容路径（通常意味着集合缺 composite 索引）
    const sortedHint =
      res && res.sortedBy === "composite-memory"
        ? "当前为兼容排序（建议为 composite 建索引）"
        : "";

    this.setData({
      activeFilter: active,
      summaryText: active ? parts.join(" · ") : "",
      totalText: totalText,
      sortedHint: sortedHint,
    });
  },

  /** 设备 chip 选项：从已加载数据里按出现顺序去重累积（不额外查库） */
  patchDeviceChips(formatted) {
    if (!formatted || !formatted.length) return;
    const cache = this._deviceCache;
    let changed = false;
    formatted.forEach((it) => {
      if (it.deviceId && cache.indexOf(it.deviceId) < 0) {
        cache.push(it.deviceId);
        changed = true;
      }
    });
    if (!changed && this.data.deviceChips.length) return;

    const chips = [{ value: "", label: "全部设备" }];
    cache.slice(0, MAX_DEVICE_CHIPS).forEach((id) => chips.push({ value: id, label: id }));
    this.setData({ deviceChips: chips });
  },

  /** source 展示名 */
  sourceLabelOf(value) {
    if (value === "device") return "设备上传";
    if (value === "manual") return "手工录入";
    return value || "";
  },

  /** 综合评分分档：≥80 绿 / 60-79 蓝 / <60 橙 */
  scoreLevelOf(composite) {
    const n = Number(composite);
    if (!isFinite(n)) return "none";
    if (n >= 80) return "high";
    if (n >= 60) return "mid";
    return "low";
  },

  /**
   * 把云端文档整理成 wxml 直接可用的结构
   * 四维：库里是 [0,1]，展示 ×100 取整
   * 细腻度：roughness 越小越细腻 → 展示时取反，避免误读方向
   */
  formatItem(item) {
    const metrics = [
      { key: "softness", label: "柔软", value: fmt.metricToPercent(item.softness) },
      { key: "smoothness", label: "顺滑", value: fmt.metricToPercent(item.smoothness) },
      { key: "roughness", label: "细腻", value: 100 - fmt.metricToPercent(item.roughness) },
      { key: "rebound", label: "回弹", value: fmt.metricToPercent(item.rebound) },
    ];
    return {
      _id: item._id,
      fabricName: item.fabricName || "未命名布料",
      deviceId: item.deviceId || "",
      composite: fmt.formatScore(item.composite), // composite 本身就是 0-100
      scoreLevel: this.scoreLevelOf(item.composite),
      createdAtText: fmt.formatTime(item.createdAt),
      startedAtText: fmt.formatTime(item.startedAt),
      source: item.source || "",
      sourceLabel: this.sourceLabelOf(item.source),
      metrics: metrics,
    };
  },

  // ======================= 小工具 / 空态动作 =======================

  /** 统一读取 data-* （wxml 里 dataset key 一律小写） */
  readDataset(e, key) {
    const ds = (e && e.currentTarget && e.currentTarget.dataset) || {};
    const v = ds[key];
    return v === undefined || v === null ? "" : v;
  },

  /** 点击单条 → 详情页（带 _id） */
  onTapItem(e) {
    const id = this.readDataset(e, "id");
    if (!id) return;
    wx.navigateTo({ url: "/pages/detail/detail?id=" + id });
  },

  /** 顶部「远程控制」入口 → 控制页（M3 新增，仅追加一个入口，不影响列表逻辑） */
  onTapControl() {
    wx.navigateTo({ url: "/pages/control/control" });
  },

  /** 空态/错误态下的「重试」：带当前条件重新查第一页 */
  onRetry() {
    this._reqSeq++; // 作废可能还在途的旧请求
    // 上次是不是因为「按综合评分排序」失败的？是的话这次退回入库时间排序，避免重试必然再失败
    const patch = { loading: false, list: [], page: 0, hasMore: true, errMsg: "" };
    if (this.data.sortFailed) {
      patch.sortValue = "createdAt";
      patch.sortFailed = false;
    }
    this.setData(patch);
    this.loadPage(0, true);
  },
});
