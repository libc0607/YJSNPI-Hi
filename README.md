# YJSNPI-Hi
关于把基于海思芯片的 IPC 模组用在飞机上录像并顺便输出一路码流给数字图传的事  

### 硬件
暂时支持两款：  
雄迈 IVG-85HF20PYA-S 模组 （海思 Hi3516EV200 + 大法 IMX307） 支持 1920x1080@30fps 录像 + 640x360@30fps 网络输出  
雄迈 IVG-85HG50PYA-S 模组 （海思 Hi3516EV300 + 大法 IMX335） 支持 2304x1296@30fps 录像 + 720x576@30fps 网络输出  
加焊一个 16P 插头，接个 TF 卡扩展板，一般同一家淘宝店都有卖的  

### 刷机（Windows 下）
连接好串口线和网线，将电脑的 IP 设置为 192.168.1.107  
在放固件的目录下运行 [tftpd32](https://tftpd32.jounin.net/)  

在串口终端里不停按 Ctrl+C，上电，中断进入U-Boot  

对于 雄迈 IVG-85HF20PYA-S 模组 （海思 Hi3516EV200 + 大法 IMX307），输入如下命令：
```
setenv bootargs "init=linuxrc mem=${osmem} console=ttyAMA0,115200 root=/dev/mtdblock1 rootfstype=squashfs mtdparts=hi_sfc:0x40000(boot),0x740000(romfs),0x80000(mtd)"

saveenv

mw.b 0x42000000 0xff 0x740000;tftp 0x42000000 <你的固件文件名>

sf probe 0;sf lock 0;sf erase 0x40000 0x740000;sf write 0x42000000 0x40000 0x740000

reset

```
对于 雄迈 IVG-85HG50PYA-S 模组 （海思 Hi3516EV300 + 大法 IMX335），咕咕咕  

### 设置文件
将扩展板通过排线接好  
TF 卡根目录中放入如下文件并按照需求修改：  

venc.ini:  
这个文件用于设置视频编码及输出；不存在文件的话开机会不启动  
帧率都是 30fps，没法设置，因为这芯片的视频输入就只能到这么高  
```
[venc]

# CH0: record & save to tf
ch0_enc=265         # enc: 264, 265
ch0_rc=6            # rc: 0(cbr),1(vbr),2(cvbr),3(avbr),6(qvbr)
ch0_kbps=5120       # kbps
ch0_gop=1           # 0-normal,1-dualp,2-smartp
ch0_res=1080        # hi3516ev200: 1080, hi3516ev300:1296 (fixed)
ch0_savedir=/tf     # Save video here

# CH1: low latency udp stream
ch1_enc=265         # enc: 264, 265
ch1_res=360         # 240, 360, 480(ev300 only)
ch1_rc=0            # rc: 0(cbr),1(vbr),2(cvbr),3(avbr),6(qvbr) - for CH1, cbr is recommended
ch1_kbps=1024       # kbps
ch1_gop=1           # 0-normal,1-dualp,2-smartp
ch1_udp_bind_port=20000         # bind port 
ch1_udp_send_ip=192.168.1.107   # send udp stream to <ch1_udp_send_ip>:<ch1_udp_send_port>
ch1_udp_send_port=5000

```

eth.ip:
摄像机的 IP 地址；这个文件里就只是一个地址，不能写别的；不支持 IPv6；不设置的话默认 192.168.1.10
```
192.168.1.20
```

将包含这两个文件的 TF 卡插入摄像机后上电，这两个文件会被自动移动到摄像机内部存储

### Web 共享 TF 卡
默认开启了 busybox 的 httpd 服务，浏览器打开你设置的摄像头 IP，进入 tf 目录即可看到  

### Telnet 能用不 
root/xmhdipc   

### 将 CH1 的视频输出作为 EZ-WifiBroadcast / OpenHD 的视频输入  
这部分不打算提供任何支持，仅提供思路，会用的自然会用  
举例：在 EZ-WifiBroadcast 的启动脚本中，找到形如 
```
"raspivid -blablabla ... | tx_rawsock -blablabla ..." 这样的一行  
```
改成 
```
"socat UDP-LISTEN:xxxxx - | tx_rawsock -blablabla ..."  
```  
将 venc.ini 中 CH1 的编码方式改为 264 （265 如果之后支持了也能用   
通过任何方式将天空端与摄像头通过网络连接起来，并可以相互 ping 通   
即可  

### 在电脑上先看看视频流？
安装 gstreamer，   
```
# H.265
gst-launch-1.0.exe udpsrc port=5000 ! h265parse ! avdec_h265 ! autovideosink sync=false

# H.264
gst-launch-1.0.exe udpsrc port=5000 ! h264parse ! avdec_h264 ! autovideosink sync=false
```

### 其他 
本固件是在雄迈原厂固件上直接修改而来，修改部分都有开源  
分享此教程时请按照 CC BY-NC-SA 3.0  
用到的海思 SDK 可以在 [这个 Telegram Channel](https://t.me/hi3516) 下载  
