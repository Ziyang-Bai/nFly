# nFly

简体中文 | [English](README.en.md)

[![Build and Release](https://github.com/Ziyang-Bai/nFly/actions/workflows/build.yml/badge.svg)](https://github.com/Ziyang-Bai/nFly/actions/workflows/build.yml)

TI-Nspire CX 上运行的果蝇大脑神经网络模拟器
TI-Nspire CX / CX II 的 Ndless 原生果蝇神经网络模拟程序，移植自 [FlyBrain](https://github.com/snedea/flybrain)。

在计算器上神经更新步长为 100 ms，行为更新步长为 500 ms，均指模拟时间。计算耗时超过步长时，模拟的实际推进速度会降低。

## 运行要求

- 64 MB 或以上内存的彩屏 TI-Nspire CX / CX II
- 不支持经典黑白机型及 32 MB 机型（包括 CM）。
- 程序和连接组数据需放在计算器的同一文件夹中。内存不足时，重启计算器后再打开。

## 下载与安装

从 [GitHub Releases](https://github.com/Ziyang-Bai/nFly/releases) 下载已有版本的 `nFly.zip`；未发布的构建可在 [Actions](https://github.com/Ziyang-Bai/nFly/actions/workflows/build.yml) 中下载成功运行的构建产物。

压缩包包含：

- `nFly.tns`：程序。
- `connectome.tns`：程序读取的连接组数据。
- `connectome.json`：来源、格式、模拟参数和校验值。
- `LICENSE`：GNU GPLv3 许可证。
- `THIRD_PARTY_NOTICES`：上游版权声明与 MIT 许可文本。

解压后按以下顺序传输：
1. 在计算器中新建文件夹，例如 `nFly`。
2. 先传入 `connectome.tns`，再传入 `nFly.tns`，两者保持在同一文件夹。
3. 从“我的文档”打开 **`nFly.tns`**，等待数据加载。不要打开 `connectome.tns`。

`connectome.json`、`LICENSE` 和 `THIRD_PARTY_NOTICES` 可保留在电脑上供查阅，无需传入计算器。

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
| H | 打开 / 关闭帮助 |
| R | 重置模拟 |
| Esc | 退出并返回系统 |

## 构建

在 Linux 环境准备 GNU Make 4.3+、Python 3 和 Ndless SDK。

在仓库根目录执行：

```sh
make check
make all release
```

构建产物：

```text
dist/nFly.tns
dist/connectome.tns
dist/connectome.json
dist/LICENSE
dist/THIRD_PARTY_NOTICES
dist/nFly.zip
```

指定输入：

```sh
make all release SOURCE=/path/to/connectome.bin.gz
```

## 生成 connectome.tns

使用 [`tools/pack_connectome.py`](tools/pack_connectome.py)

### 下载并转换

```sh
python3 tools/pack_connectome.py
```

### 使用离线输入

执行：

```sh
python3 tools/pack_connectome.py \
  --source /path/to/connectome.bin.gz \
  --output dist/connectome.tns
```

### 校验

- 上游仓库：[snedea/flybrain](https://github.com/snedea/flybrain)。
- 源码修订：[`9191824d17871b7851645782d53d23f213ddb938`](https://github.com/snedea/flybrain/tree/9191824d17871b7851645782d53d23f213ddb938)。
- 原始文件：[data/connectome.bin.gz](https://raw.githubusercontent.com/snedea/flybrain/9191824d17871b7851645782d53d23f213ddb938/data/connectome.bin.gz)。
- SHA-256：

```text
fbf8d440ca1207c7573e1acdd2366f9d0beb9b533c1710f21681264f81b1cc49
```

## 来源和许可证

nFly 代码使用 [GNU 通用公共许可证 v3.0](LICENSE)（GPL-3.0-only）。FlyBrain 的原始 MIT 版权与许可声明保留在 [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES) 中。

连接组来源为 **FlyWire FAFB v783**，通过 FlyBrain 数据文件取得。数据的使用和引用要求请参阅 [FlyWire](https://flywire.ai/) 及以下论文：

Dorkenwald et al., *Neuronal wiring diagram of an adult brain*, Nature **634**, 124–138 (2024). [doi:10.1038/s41586-024-07558-y](https://doi.org/10.1038/s41586-024-07558-y)
