# AppOpsTUI
简单的cmd AppOps封装，使用Termbox2库
依赖Termux + Root身份运行

## 安装
1. 下载[二进制文件](https://github.com/Shiranosama/AppOpsTUI/releases)与[appops_descriptions.txt](https://raw.githubusercontent.com/Shiranosama/AppOpsTUI/refs/heads/main/appops_descriptions.txt)
2. 新建目录/storage/emulated/0/AppOps/AppOpsDesc，并将appops_descriptions.txt放入其中
3. 把二进制文件存进Termux可访问的地方

## 使用

### Termux运行
```bash
sudo /path/to/appops
```

- 如果你没有sudo，请安装
```bash
pkg install tsu
```

## 编译
```bash
make
```

## 声明
- 本项目纯个人使用，不一定会维护
- 本项目使用LLM辅助开发
