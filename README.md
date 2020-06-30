# HttpServer
本项目是基于《TCP/IP网络编程》、《Linux高性能服务器编程》书籍开发的一个简单HTTP服务端程序
采用主线程+工作线程的方式进行开发，主线程采用epoll方式负责监听和接受连接，工作线程负责I/O操作。
事件的分发采用生产者-消费者模型，实现了线程安全
Web服务器实现了GET请求，支持静态资源的访问，支持优雅关闭连接


待能力长进时再增加事件处理模式以及增加新的方法和功能予以完善，以便它能支持高并发、多功能
