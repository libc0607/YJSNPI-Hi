# YJSNPI-Hi
关于把基于海思芯片的 IPC 模组用在飞机上录像并顺便输出一路码流给数字图传的事  
本项目为 [YJSNPI-Broadcast](https://github.com/libc0607/YJSNPI-Broadcast) 填坑过程中的副产物，但由于单独用也能用就另开了个 repo

### 硬件
暂时支持两款：  
雄迈 IVG-85HF20PYA-S 模组 （海思 Hi3516EV200 + 大法 IMX307） 支持 1920x1080@30fps 录像 + 640x360@30fps 网络输出  
![307](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/307.png)  
雄迈 IVG-85HG50PYA-S 模组 （海思 Hi3516EV300 + 大法 IMX335） 支持 2304x1296@30fps 录像 + 720x576@30fps 网络输出  
![335](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/335.png)  

购买时建议配好镜头；不玩夜航的话光圈 F2.0 足够了，镜头焦距越小视角越大畸变越大  
也可以买变焦镜头，但这还只是另一个咕咕咕着的坑  

同时需要购买：  
1. 雄迈模组配套的 TF 扩展板，一般同一家店会有卖的，问问老板  
2. 16P 0.5mm FPC 插座，如 立创商城 C11071  
3. 16P 0.5mm FPC 排线（反向）  
4. 1.25mm 3P 直插座及配套的线，用于接串口  
5. 雄迈模组用的尾线，同一家店会有卖的+1  
6. M2 铜柱、螺丝、螺母若干  
网线、电脑、串口模块、读卡器啥的默认有  

焊接及连接方法看图：  
![fpc1](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/fpc-conn.png)  
![fpc2](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/fpc-conn-finish.png)  
![fpc3](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/fpc-connect.png)  
![uart1](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/uart-conn.png)  
![uart2](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/uart-conn-finish.png)  
![eth1](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/eth-power-conn.png)  
![conn](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/conn-1.png)  


### 刷机（Windows 下）
连接好串口线和网线，将电脑的 IP 设置为 192.168.1.107  
下载 image/ 目录下对应型号的固件  
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
注：这里改动了原厂分区，因为那个分区方式太麻烦了。。。  
![bootargs](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/bootargs.png)  
![tftp](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/flash-tftp.png)  
![erase](https://github.com/libc0607/YJSNPI-Hi/raw/master/pics/flash-erase.png)  
对于 雄迈 IVG-85HG50PYA-S 模组 （海思 Hi3516EV300 + 大法 IMX335），咕咕咕  

### 设置文件
将扩展板通过排线接好  
TF 卡根目录中放入如下文件并按照需求修改：  

venc.ini:  
这个文件用于设置视频编码及输出；  
如果刚刷完机，在内部存储和tf卡中都不存在文件的话，开机就不启动录像；tf卡根目录中有此文件则更新至内部存储；tf卡中没有但内部有保存则使用内部    

帧率：都是 30fps，没法设置，因为这芯片的视频输入就只能到这么高，对于玩飞机已经够低了   
chX_rc：码率控制，只有第一个 CBR 是恒定码率，由于数字图传空中传输的特性最好在 CH1 设置成这个模式；CH0可以设置为其他模式节约一点点空间    
chX_gop：P帧参考啥啥啥，海思文档推荐在运动场景下用dualp    
ch0_res：写死了一些参数，这里ev200写死1080，ev300写死1296即可     
ch1_res：这里3516ev200最高只能设置到360P，原因是按照原厂参数来算，再大的话预留给编码的内存就不足了（你也可以改uboot中的osmem参数）  
ch0_savedir 保存位置：开机后会自动挂载tf卡的第一个分区到 /tf，如果像下面这样写就是保存到卡的根目录     
awb_speed_custom 和 awb_speed： 影响不同光源色温的场景间转换时自动白平衡收敛的速度，比如从室内飞到室外的时候  

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

awb_speed_custom=1  # 0-disable, 1-enable
awb_speed=2048      # 0~4095

```

eth.ip:  
摄像机的 IP 地址；  
这个文件里就只是一个地址，不能写别的；  
不支持 IPv6；不设置的话默认 192.168.1.10  
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
将 venc.ini 中 CH1 的编码方式改为 264  
265 取决于地面端是否支持解码，对空中传输透明；如果你的地面可以想办法解码的话那 265 当然吼哇  

最后 通过任何方式将天空端与摄像头通过网络连接起来，并可以相互 ping 通   
即可  

### 在电脑上先看看视频流？
安装 gstreamer，   
```
# H.265
gst-launch-1.0.exe udpsrc port=5000 ! h265parse ! avdec_h265 ! autovideosink sync=false

# H.264
gst-launch-1.0.exe udpsrc port=5000 ! h264parse ! avdec_h264 ! autovideosink sync=false
```

### 淘宝买到的扩展板怎么画了个天线并联？？
嫌丑的话可以参考我在 OSHWHUB 的 [这个项目](https://oshwhub.com/libc0607/xm-ext-board-fpv) 自己画一个  

### 其他 
本固件是在雄迈原厂固件上直接修改而来，修改部分都有开源  
分享此教程时请按照 CC BY-NC-SA 3.0  
用到的海思 SDK 可以在 [这个 Telegram Channel](https://t.me/hi3516) 下载  
