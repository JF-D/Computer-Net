# 命令
应用移植实现为简单实现，使用命令为：

1. 运行构建的拓扑文件
2. 在h2, h3上执行`./tcp_stack server 10001`
3. 在h1上执行`./tcp_stack client 10.0.0.2 10001 10.0.0.3 10001`

# Note
实现为简单的实现，`workers.conf`为连接的server的配置文件，`war_and_peace.txt`为输入，均在代码内部显式使用，未使用输入方式确定。