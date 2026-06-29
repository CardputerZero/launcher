# ADB Shell 终端高亮与复制 / ADB Shell Highlighting & Copy

> 关于 `adb shell` 里"高亮显示"与"复制选中内容"是怎么回事、以及在 CardputerZero 上是如何实现的。
> How highlighting and copy-selection work in `adb shell`, and how it is wired up on CardputerZero.

---

## 一、先分清两种"高亮" / Two different kinds of "highlight"

在 `adb shell` 里你会看到两类完全不同的高亮，它们由**不同的一方**负责：

| 高亮类型 | 例子 | 谁负责 | 在哪实现 |
| --- | --- | --- | --- |
| **选区 / 复制高亮** | 鼠标拖选一段文字，被选中的字符反白 | **宿主终端**（macOS Terminal / iTerm2 / VS Code 终端 …） | 主机端，设备无需任何代码 |
| **内容着色高亮** | `ls` 目录名变蓝、`grep` 命中变红、提示符带颜色 | **设备上的 shell（bash）** | 设备端 `/root/.bashrc` |

> 一句话：**"复制时反白"是你电脑上的终端 App 做的；"输出内容五颜六色"才是设备这边配置的。**
>
> In short: **selection/copy highlight is your computer's terminal app; the colored output is what the device configures.**

---

## 二、复制选中内容的高亮（宿主侧）/ Copy-selection highlight (host side)

### 原理 / How it works

`adb shell` 进入交互模式时，`adbd` 在设备上分配一个 **PTY（伪终端）**，把 bash 的输出当作**纯字节流**通过 USB 传回主机。主机上的 `adb` 客户端把这串字节交给你正在用的**终端模拟器**（Terminal.app、iTerm2、VS Code 集成终端等）。

- 你**拖动鼠标选择**文字时，是终端 App 在它自己的渲染缓冲区里反白并复制 —— 这一步**完全发生在主机**，设备根本不知道你在选什么。
- 所以"能不能高亮选中并复制"取决于你用的终端 App，而**不是** CardputerZero。任何正常的终端都支持。

The interactive `adb shell` runs over a **PTY** on the device; `adbd` ships bash's output as a raw byte stream over USB. Your host **terminal emulator** renders it. When you drag-select text, the terminal app highlights and copies it **locally on the host** — the device is not involved at all. So selection/copy works in any normal terminal regardless of the device.

### 设备端只需保证两点 / The device only has to guarantee two things

1. **真正的 PTY**：交互式 `adb shell` 会跑一个 *login shell*，得到真 PTY，方向键、Ctrl-C、行编辑才正常。
2. **正确的换行/回绕**：`shopt -s checkwinsize` 让 bash 在窗口尺寸变化后更新 `$LINES/$COLUMNS`，避免选区错位、复制出多余换行。

1. A **real PTY** (interactive `adb shell` is a login shell → real PTY → arrow keys / line editing work).
2. **Correct wrapping**: `shopt -s checkwinsize` keeps `$LINES/$COLUMNS` in sync so selections don't misalign.

### 复制更顺手的小技巧 / Tips for cleaner copies

- **按列/块选择**：iTerm2 / 多数终端按住 <kbd>Option/Alt</kbd> 拖动可做"矩形选择"，复制表格不带左边的提示符。
- **想连提示符一起干净复制**：可在登录后 `PS1=''` 或临时 `unset PROMPT_COMMAND`，但通常没必要。
- **大段输出**：用 `adb shell 'cmd' | pbcopy`（macOS）直接把结果拉到剪贴板，比鼠标拖选更可靠。

---

## 三、内容着色高亮（设备侧）/ Output colorization (device side)

这部分是 CardputerZero 真正"实现"的东西。Debian 默认 `adb shell` 进去是 **dash**，**没有颜色、没有补全、方向键是乱码**。我们在 `cardputer-adb` 启用 ADB 时做了一套终端增强。

This is what CardputerZero actually implements. By default Debian's `adb shell` lands in **dash** (no color, no completion, arrow keys print escape codes). When ADB is enabled, `cardputer-adb` installs a terminal upgrade.

### 实现位置 / Where it lives

设备脚本：`/usr/share/APPLaunch/adb/cardputer-adb`（源码在 `projects/APPLaunch/APPLaunch/adb/cardputer-adb`），函数 `setup_better_shell()` 会幂等地写入：

- `~/.profile`：交互式 `adb shell`（dash 登录 shell）时 `exec /bin/bash`，换成 bash。
- `~/.bashrc`：设置 `TERM`、加载 bash-completion、历史、彩色提示符，以及**彩色输出**。

`adbd` runs as **root**, so the files are `/root/.profile` and `/root/.bashrc`.

### `~/.profile`：把 dash 升级成 bash / Upgrade dash → bash

```sh
# >>> cardputer-adb shell >>>
if [ -z "${BASH:-}" ] && [ -x /bin/bash ]; then
    case $- in
        *i*) exec /bin/bash ;;   # 仅交互式 login shell 才切 bash
    esac
fi
# <<< cardputer-adb shell <<<
```

> `adb shell <cmd>` 走的是非交互 `sh -c`，**不读** `.profile`，所以脚本仍跑在快速的 dash 上，不受影响。
> Non-interactive `adb shell <cmd>` does not read `.profile`, so scripts keep running on fast dash.

### `~/.bashrc`：颜色与补全 / Colors and completion

```sh
# >>> cardputer-adb shell >>>
case "$TERM" in ""|dumb|unknown) export TERM=xterm-256color ;; esac
# bash-completion（Tab 补全命令/路径/参数）
if ! shopt -oq posix; then
    if [ -f /usr/share/bash-completion/bash_completion ]; then
        . /usr/share/bash-completion/bash_completion
    elif [ -f /etc/bash_completion ]; then
        . /etc/bash_completion
    fi
fi
HISTSIZE=2000; HISTFILESIZE=4000
shopt -s histappend checkwinsize 2>/dev/null
# 彩色提示符：绿色 user@CardputerZero + 蓝色路径
PS1='\[\e[1;32m\]\u@CardputerZero\[\e[0m\]:\[\e[1;34m\]\w\[\e[0m\]\$ '

# 输出着色：目录列表 / grep 命中 / diff 等
if [ -x /usr/bin/dircolors ]; then
    eval "$(dircolors -b 2>/dev/null)"   # 生成 LS_COLORS
fi
alias ls='ls --color=auto'
alias ll='ls -alF --color=auto'
alias la='ls -A --color=auto'
alias grep='grep --color=auto'
alias fgrep='fgrep --color=auto'
alias egrep='egrep --color=auto'
alias diff='diff --color=auto'
alias ip='ip -color=auto'
export CLICOLOR=1
export GREP_COLORS='ms=01;31:mc=01;31:sl=:cx=:fn=35:ln=32:bn=32:se=36'
export LESS='-R'   # 让 less / git 分页器透传 ANSI 颜色
# <<< cardputer-adb shell <<<
```

### 着色用到的机制 / The mechanisms behind the colors

| 元素 | 机制 |
| --- | --- |
| `TERM=xterm-256color` | 告诉程序终端支持 256 色，是一切 ANSI 颜色的前提 |
| `LS_COLORS`（`dircolors -b`） | `ls --color` 按文件类型/权限决定颜色（目录蓝、可执行绿、链接青…） |
| `--color=auto` 别名 | `ls/grep/diff/ip` 仅在输出到 TTY 时上色，重定向到文件时自动关闭 |
| `GREP_COLORS` | 自定义 `grep` 命中颜色（命中=亮红，文件名=紫，行号=绿） |
| `PS1` 中的 `\e[1;32m` … | ANSI 转义序列直接给提示符上色 |
| bash-completion | Tab 补全（命令、路径、`git`/`systemctl` 等子命令） |
| `LESS=-R` | 分页时保留颜色（否则 `git log`、`man` 颜色会变成 `ESC[...` 乱码） |

> 注意：颜色是 **ANSI 转义序列**，本质也是字节流的一部分，照样通过 PTUSB 传到主机由终端 App 渲染——所以**着色和复制高亮其实共用同一条通路**，只是一个由设备产生、一个由主机产生。

---

## 四、快速验证 / Quick check

ADB 打开后，在设备上 `adb shell` 进入，应看到：

```
root@CardputerZero:~#        ← 提示符有颜色
# ls /            → 目录蓝色、可执行绿色
# grep root /etc/passwd   → "root" 命中亮红
# ls --c<Tab>     → 补全成 --color
```

确认设备配置已生效：

```bash
adb shell 'echo $TERM; alias ls; echo $LS_COLORS | head -c 40'
# 期望：xterm-256color / alias ls='ls --color=auto' / 一串 LS_COLORS
```

---

## 五、常见问题 / Troubleshooting

- **完全没颜色**：多半是用了 `adb shell <cmd>`（非交互，不读 bashrc）。直接 `adb shell` 进交互模式再跑命令。
- **颜色变成 `ESC[0;34m` 之类乱码**：宿主终端 `TERM` 不支持，或经过了不透传颜色的管道；交互终端里一般不会出现。
- **`git log` / `man` 翻页后颜色丢失**：确认 `LESS=-R` 已生效（本配置已设置）。
- **改了脚本但设备没变**：`~/.bashrc` 的写入是**幂等**的（靠 `# >>> cardputer-adb shell >>>` 标记），已存在就不会重复追加。要更新现有设备，需先删掉标记之间的旧块再重新 `cardputer-adb enable`，或手动同步。

---

## 六、English summary

- **Two highlights**: (1) *selection/copy highlight* is rendered by your **host terminal** over the PTY byte stream — works in any terminal, nothing to implement on the device; (2) *output colorization* (`ls`/`grep`/prompt) is configured **on the device** in `/root/.bashrc`.
- **Implementation**: `cardputer-adb`'s `setup_better_shell()` upgrades the interactive `adb shell` from dash to **bash** (`~/.profile`), then in `~/.bashrc` sets `TERM=xterm-256color`, loads **bash-completion**, a colored `PS1`, `LS_COLORS` via `dircolors`, `--color=auto` aliases for `ls/grep/diff/ip`, `GREP_COLORS`, and `LESS=-R` so pagers keep colors.
- **Non-interactive `adb shell <cmd>`** stays on dash for speed and is unaffected.
