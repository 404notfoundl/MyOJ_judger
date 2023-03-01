<!--
 * @Author: 
 * @Date: 2022-03-19 16:01:44
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2023-02-26 21:01:41
 * @Description: 请填写简介
-->
# MyOJ_judger

评测机部分，基于 `cpp` 开发。
目前只支持评测 `c/cpp`
于容器位置 `/root/projects`
[前端]()
[后端]()

## 部署
  * 参照 `server` 部分
    由于此部分需要部分权限，目前创建容器需要附加参数 `privileged`。
    待查明所需具体权限后进行进一步限制 
### 配置
建议进入容器修改以下选项
1. `ssh-key`
    ```shell
    ssh-keygen -t rsa
    # server 容器需先启动ssh
    scp ~/.ssh/id_rsa.pub rsync_u@[backend-host]:~/.ssh/authorized_keys
    # 或手动复制 id_rsa.pub 到 server 容器中 rsync_u 的 authorized_keys 中
    ```
2. `/etc/rsnapshot.conf`
    ```shell
    ssh_args        -p 2022 # server端的ssh端口
    backup  rsync_u@[backend-host]:/home/problems_backups     prob_backup/ # server端的地址
    ```
    * 使用 `rsnapshot backup` 测试同步文件

## 编译
于项目根目录
```shell
make clean # 清除已有文件
make # 默认开启O2 或：
make OPT=-g # 附加调试信息
```
## 启动
  ```shell
  cd dispatcher
  sh start_judgerd.sh &
  ```

## 相关配置
`judger/main.cpp`
```cpp
judger::compiler::args::options; // 编译选项
```
### 关闭 SECCOMP
于 `judger/makefile` 添加宏定义 `SCMP_OFF` 重新编译共享库后重启程序即可关闭
```shell
make clean # 清除相应 .o 文件
make # 编译共享库
```
