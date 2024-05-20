# http 服务器的实现
初步实现功能，与前端结合能够实现上传下载文件的功能。\
对接收到的不存在的HTTP地址，能够正确响应，发回404.html以及配套的JavaScript和CSS文件。\
设置了外部变量directory，如果编译时没有指定directory，就会以本地的服务器的根目录为默认地址。\
注意代码中并没有实现不存在downloaded文件夹程序自动创建的功能，所以在运行代码的过程中，需要用户在对应的文件夹下创建名为downloaded的文件夹作为上传以及下载的目标文件夹。\
[![pkKWoU1.md.png](https://s21.ax1x.com/2024/05/20/pkKWoU1.md.png)](https://imgse.com/i/pkKWoU1)
[![pkKWT4x.md.png](https://s21.ax1x.com/2024/05/20/pkKWT4x.md.png)](https://imgse.com/i/pkKWT4x)
# 关于服务器功能的升级实现
实现了线程池和epoll的复用。\
在此基础是实现了管理线程（负责线程数的扩容与缩减）、任务队列（负责向任务队列填装任务）、工作线程（负责从任务队列取任务并处理）。 其中任务队列、工作线程采用生产者-消费者模型。采用互斥量+条件变量实现线程同步。
# 存在的问题
在测试过程中发现前端发送html文件到服务器端有部分错误（服务器能够成功下载，并且在本地文件夹内可以发现该完整文件，但是前端会发出不带任何错误代码的回馈，而是发出禁止警告），可能是前端读取到文件中了不同版本的http协议而发出错误警告。\
国内博客地址：https://blog.csdn.net/m0_60908355/article/details/139026414?spm=1001.2014.3001.5502
[![pkKWHC6.md.png](https://s21.ax1x.com/2024/05/20/pkKWHC6.md.png)](https://imgse.com/i/pkKWHC6)
