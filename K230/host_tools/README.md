# 定制 CanMV IDE 代码

`canmv_vscode/` 保存本地 K230 开发工具的现行状态管理代码：

- `extension.js` 是当前使用的扩展运行 bundle，`package.json` 保留命令启用条件。它们来自已安装的 CanMV 0.9.10 扩展并经过本地修改。
- `native/go/` 是扩展后端的完整 Go 模块源码，基础版本为上游提交 `326c1d6`，包含本地连接观察、断开处理和运行接管修改。可在此目录使用 Go 构建 `./cmd/canmv-backend`。
- `LICENSE` 保留上游版权及许可。

这些文件用于复现本地改动及执行 `K230/tests/test_ide_state_management.py`。这里没有完整的 VS Code 扩展资源目录、打包工程或预编译后端；单独复制本目录不能安装一个完整扩展，需在相同版本的完整上游扩展中集成修改。

主机代码不上传到 SD 卡。生产应用、调试应用与固件源文件位于 `K230/` 和 `K230/native/`。
