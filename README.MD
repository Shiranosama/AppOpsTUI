# AppOpsTUI
- 简单的cmd AppOps封装，使用Termbox2库
- 依赖Termux + Root身份运行

## 安装
1. 下载[二进制文件](https://github.com/Shiranosama/AppOpsTUI/releases)与[appops_descriptions.txt](https://raw.githubusercontent.com/Shiranosama/AppOpsTUI/refs/heads/main/appops_descriptions.txt)
2. 确保授予Termux储存权限
3. 新建目录/storage/emulated/0/AppOps/AppOpsDesc，并将appops_descriptions.txt放入其中
4. 把二进制文件存进Termux可访问的地方

## 使用

### Termux运行
```bash
sudo /path/to/appops
```

- 如果你没有sudo，请安装
```bash
pkg install tsu
```

## Help
- 软件包列表：
  + ⬆️/⬇️或者k/j: 在列表中移动
  + 回车: 编辑选中软件包
  + /: 搜索模式
  + r: 刷新列表渲染
  + Ctrl+G: 放弃所有更改并刷新状态
  + ESC/q/Ctrl+C: 返回软件包列表
- 编辑界面：
  + ⬆️/⬇️或者k/j: 在列表中移动
  + 空格/回车: 循环切换选中权限状态
  + s: 保存更改
  + b: 备份原始状态
  + o: 恢复备份
  + ESC/q/Ctrl+C: 返回软件包列表
  + Ctrl+U: 显示描述详情

## 编译
- 需要aarch64-linux-gnu-g++
```bash
make
```

## 声明
- 本项目纯个人使用
- 本项目使用LLM辅助开发
