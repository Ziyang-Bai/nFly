# nFly

[![Build and Release](https://github.com/Ziyang-Bai/nFly/actions/workflows/build.yml/badge.svg)](https://github.com/Ziyang-Bai/nFly/actions/workflows/build.yml)

TI-Nspire CX / CX II 的 Ndless 原生果蝇神经网络模拟程序，移植自 [FlyBrain](https://github.com/snedea/flybrain)。

保留固定上游图中的 **139,255 个神经元、2,698,236 条连接和 63 个神经群**，在计算器上计算神经发放，并提供食物、触碰、风、光照、温度和危险气味交互。运动沿用上游的虚拟腹神经索（VNC）模型，将下行神经活动与内部驱动力映射到腿、翅、口器等输出。

神经更新步长为 100 ms，行为更新步长为 500 ms，均指模拟时间。计算耗时超过步长时，模拟的实际推进速度会降低。

## 运行要求

- 64 MB 或以上内存的彩屏 TI-Nspire CX / CX II，安装与系统版本匹配的 [Ndless](https://ndless.me/)。
- 不支持经典黑白机型及 32 MB 机型（包括 CM）。
- 程序和连接组数据需放在计算器的同一文件夹中。内存不足时，关闭其他 Ndless 程序或重启计算器后再打开。

## 下载与安装

从 [GitHub Releases](https://github.com/Ziyang-Bai/nFly/releases) 下载已有版本的 `nFly.zip`；未发布的构建可在 [Actions](https://github.com/Ziyang-Bai/nFly/actions/workflows/build.yml) 中下载成功运行的构建产物。

压缩包包含：

- `nFly.tns`：程序。
- `connectome.tns`：程序读取的连接组数据。
- `connectome.json`：来源、格式、模拟参数和校验值。
- `LICENSE`：代码许可证及上游署名。

解压后按以下顺序传输：

1. 在计算器中新建文件夹，例如 `nFly`。
2. 先传入 `connectome.tns`，再传入 `nFly.tns`，两者保持在同一文件夹。
3. 从“我的文档”打开 **`nFly.tns`**，等待数据加载。不要打开 `connectome.tns`。

`connectome.json` 和 `LICENSE` 可保留在电脑上供查阅，无需传入计算器。

## 操作

| 按键 | 功能 |
| --- | --- |
| 方向键 / 触摸板方向 | 移动场景光标；神经群视图中选择神经群 |
| Enter | 在光标位置放置食物 |
| F，然后 Enter | 将光标移到果蝇位置并投食 |
| 1 / 2 / 3 / 4 | 触碰头 / 胸 / 腹 / 腿 |
| A / Shift+A | 轻风 / 强风，方向由光标与果蝇的相对位置决定 |
| L | 切换光照 |
| T | 切换温度 |
| D | 开关危险气味 |
| C | 清除食物 |
| P | 暂停 / 继续 |
| B | 切换场景 / 神经群视图 |
| H | 打开 / 关闭帮助，显示帮助时暂停模拟 |
| R | 重置模拟 |
| Esc | 退出并返回系统 |

## 构建

在 WSL/Linux 环境准备 GNU Make 4.3+、Python 3 和 Ndless SDK，并将 SDK 的 `bin` 目录加入 `PATH`，使 `nspire-gcc`、`genzehn` 和 `make-prg` 可用。主机测试还需要 C99 编译器、zlib 开发文件和 libm。

在仓库根目录执行：

```sh
make check
make -j2 all release
```

`make check` 运行神经网络、世界行为和渲染三个主机测试。构建生成：

```text
dist/nFly.tns
dist/connectome.tns
dist/connectome.json
dist/LICENSE
dist/nFly.zip
```

`nFly.zip` 包含其余四个文件。首次构建会下载固定版本的上游数据；已有离线输入时可执行：

```sh
make -j2 all release SOURCE=/path/to/connectome.bin.gz
```

`SOURCE` 仅在数据打包规则运行时使用；如需重新生成已有的数据文件，请使用下面的脚本命令。源码仓库不包含生成的程序、连接组数据及本地构建缓存。

## 生成 connectome.tns

[`tools/pack_connectome.py`](tools/pack_connectome.py) 只使用 **Python 3 标准库**，无需安装 Python 包，也不依赖 Ndless SDK。以下命令均在仓库根目录运行。

### 自动下载并转换

```sh
python3 tools/pack_connectome.py
```

默认输入为 `.cache/connectome.bin.gz`，输出为 `dist/connectome.tns` 和同名 JSON 旁文件 `dist/connectome.json`。输入文件不存在时，脚本自动从固定上游地址下载并缓存；输入已存在时直接读取。输出目录会自动创建，已有输出会被覆盖，JSON 报告也会打印到终端。

Windows 安装 Python 3 后可在 PowerShell 中运行：

```powershell
python tools/pack_connectome.py
```

### 使用离线输入

预先取得下列固定版本的原始 `connectome.bin.gz`，再执行：

```sh
python3 tools/pack_connectome.py \
  --source /path/to/connectome.bin.gz \
  --output dist/connectome.tns
```

离线使用时，`--source` 指定的文件必须已存在；路径不存在仍会触发下载。`--output` 可指定其他输出路径，JSON 路径由其后缀替换为 `.json` 得到。安装时数据文件仍须命名为 `connectome.tns`。

### 固定来源与校验

- 上游仓库：[snedea/flybrain](https://github.com/snedea/flybrain)。
- 源码修订：[`9191824d17871b7851645782d53d23f213ddb938`](https://github.com/snedea/flybrain/tree/9191824d17871b7851645782d53d23f213ddb938)。
- 原始文件：[data/connectome.bin.gz](https://raw.githubusercontent.com/snedea/flybrain/9191824d17871b7851645782d53d23f213ddb938/data/connectome.bin.gz)。
- 原始压缩文件 SHA-256：

```text
fbf8d440ca1207c7573e1acdd2366f9d0beb9b533c1710f21681264f81b1cc49
```

下载和本地输入都必须通过这个 SHA-256 校验，不匹配时脚本报错，不接受其他版本。脚本还检查解压后的图规模、二进制长度、神经群编号及边端点。

### 转换格式

脚本先解压原始图，将神经元按神经群编号稳定排序（同群内保留上游索引顺序），重新映射边的两端，再按源神经元组织为 CSR。全部神经元和连接均保留，同一源神经元的出边保持原始遍历顺序。

每条边的权重转换为：

```text
float32((原始 float32 权重 / 全图原始权重绝对值最大值) × 0.15)
```

该规则保留权重正负号，将最大绝对值缩放到约 0.15，并以 float32 存储。

输出是 gzip 压缩的、小端序 `NFLYCSR1` 容器，以 `.tns` 后缀便于传输到计算器。解压后依次为：

1. 8 字节 `NFLYCSR1` 标记，以及三个 `uint32`：神经元数、边数、神经群数。
2. `uint32 group_offsets[groups + 1]`。
3. `uint32 csr_rows[neurons + 1]`。
4. `uint32 targets[edges]`。
5. `float32 weights[edges]`。

gzip 不写入原始文件名，时间戳固定为 0，压缩级别为 9。JSON 报告记录来源修订、格式、各群大小、模拟参数及文件大小；`source_sha256` 对应上游压缩文件，`output_sha256` 对应实际生成的 `connectome.tns`。

不同 zlib 版本可能产生不同的 gzip 字节流；校验生成文件时使用同次生成的 JSON 中的 `output_sha256`。

## CI/CD

GitHub Actions 在 `ubuntu-24.04` 上使用 [`Ziyang-Bai/setup-ndless@v1`](https://github.com/Ziyang-Bai/setup-ndless) 配置工具链：

- Push、Pull Request 和手动运行均执行 `make check`，再运行 `make -j2 all release`。
- 上传 `dist` 中的程序、数据、JSON、许可证和 ZIP，构建产物保留 14 天。
- 推送 `v*` 标签时，仅在构建成功后由独立发布任务创建 GitHub Release 并附加上述文件。
- 构建任务使用只读仓库权限，发布任务单独授予 `contents: write`。

## 来源与许可证

nFly 代码使用 [MIT License](LICENSE)，保留上游的 Seth Miller 版权声明。

连接组来源为 **FlyWire FAFB v783**，通过上述固定版本的 FlyBrain 数据文件取得。数据的使用和引用要求请参阅 [FlyWire](https://flywire.ai/) 及以下论文：

Dorkenwald et al., *Neuronal wiring diagram of an adult brain*, Nature **634**, 124–138 (2024). [doi:10.1038/s41586-024-07558-y](https://doi.org/10.1038/s41586-024-07558-y)
