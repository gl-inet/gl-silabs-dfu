# gl-silabs-dfu

本工程基于 silicon lab **xmodem-uart-bootloader** 和 silicon lab **bgapi-uart-bootloader** 进行固件升级。



## Getting Start

```shell
$ git clone https://gitlab.com/gl.iot/gl-silicon-dfu.git
```



## Run gl-silabs-dfu:

### xmodem 

```shell
gl-silabs-dfu xmodem [Upgrade file path] [Uart] [Reset IO] [DFU enable IO] [-v] [-c]
```

- **-v**: Printing the Upgrade Progress
- **-c**: Thread version checking
- **-cb**:BLE version checking

### bgapi

```
gl-silabs-dfu bgapi [dfu mode] [Upgrade file path] [Uart] [Reset IO] [DFU enable IO] [-v]
```

#### dfu mode

- **soft**: software reboot triggers the dfu mode. In this mode, you don't need input a dfu-enable IO.

​		**Warning**: If your device is only software-enabled in DFU mode, any interruption during the upgrade will cause the firmware to mess up and work only by reburning the firmware. So we don't recommend using this mode.

- **hard**: hardware triggers the dfu mode. In this mode, you need input a dfu-enable IO.



## Example

### xmodem

```shell
# gl-silabs-dfu xmodem ./gl-efr32mg21f768-thread-rcp-v2021-10-19-1fbf6bb-71e4df982.gbl /dev/ttyUSB0 2 17 -v
file size:80040
start upload...
process:[====================================================================================================>]100% 
upload ok. total size:80128
reset chip now...
dfu success!
```

### bgapi soft mode

```shell
# gl-silabs-dfu bgapi soft ./test-high.gbl /dev/ttyS1 5 -v
Get upgrade firmware size: 214332
* System boot evrnt!
* Module firmware version: 2.13.10
* Build number:            423
process:[====================================================================================================>]99% *** DFU END! ***
* System boot evrnt!
* Module firmware version: 3.3.2
* Build number:            406
Module reset finish, please check firmware version.
```



## GBL File Creation

### 环境部署
找到commander.exe的安装路径，并将该路径添加到系统Path环境变量,如下图：

![avatar](doc/img/commander路径.png)


### 生成未加密的GBL文件
打开工程文件，找到生成的 .s37 文件的路径，并在该路径下打开CMD命令行窗口，输入如下命令格式：

```shell
$ commander gbl create <gblfile> --app <filename> [--encrypt <keyfile>]
```

如下图所示，在当前目录下生成了未加密的GBL文件

![avatar](doc/img/gbl文件生成.png)