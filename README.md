# Github Host
## 使用场景
无法访问 github 相关的域名且没有root权限修改 `/etc/hosts`，无法安装代理软件
## 要求

- [LibCurl](https://curl.se/libcurl/)
## 使用
```
gcc -shared -fPIC -ldl -lcurl addrinfo.c hashmap.c cJSON.c -o addrinfo.so
```
在需要使用的终端窗口执行
```
# /to/path/addrinfo.so 为 addrinfo.so 所在位置 
# 若需要长期使用，可添加到.bashrc当中
export LD_PRELOAD=/to/path/addrinfo.so
```
当不需要使用时，可以通过下面命令关闭
```
unset LD_PRELOAD
```

## 声明
本项目使用了以下开源库：
- [cJSON](https://github.com/DaveGamble/cJSON) - Hash map implementation in C.
- [hashmap.c](https://github.com/tidwall/hashmap.c) - Ultralightweight JSON parser in ANSI C.
本项目使用的host为 [GitHub520](https://github.com/521xueweihan/GitHub520) 所提供