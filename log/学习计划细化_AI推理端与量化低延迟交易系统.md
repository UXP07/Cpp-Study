# AI推理端 与 量化低延迟交易系统 —— 细化学习指南

> 原则：**由下层到上层、由浅入深**。先打地基（数学 → C++/体系结构/OS/网络），再分方向向上攀升，沿途补充"训练端"与"量化概念"等邻域知识，最后靠开源项目融会贯通。

---

## 第〇部分：数学基础（该学什么、学到什么深度）

两个方向对数学的要求不同，不必贪多，按需学习。

### 0.1 共同必修
| 数学内容 | 学到什么程度 | 为什么需要 | 推荐资源 |
| :--- | :--- | :--- | :--- |
| **线性代数** | 矩阵乘法、转置/分块、特征值、SVD、向量化思维 | 一切数值计算的骨架；理解 GEMM 为何是性能核心 | 3Blue1Brown《线性代数的本质》+ Strang《Introduction to Linear Algebra》 |
| **概率与统计** | 期望、方差、分布（正态/指数/重尾）、分位数、假设检验、大数定律 | 性能延迟建模（P99 是分位数概念）、量化收益分布 | 《概率论与数理统计》茆诗松 或 MIT 6.041 |
| **数值分析（入门）** | 浮点标准 IEEE754（单/双精度、舍入误差）、数值稳定性 | 理解 FP16/BF16/FP8 的精度来源，量化误差分析 | 《What Every Computer Scientist Should Know About Floating-Point Arithmetic》 |
| **微积分/优化（概念级）** | 梯度、链式法则、凸优化基本概念、梯度下降 | 理解训练端如何工作即可，不必深挖 | 3Blue1Brown《微积分的本质》+ 《深度学习》(花书) 第4、8章 |

### 0.2 AI推理方向加修
- **量化数学（重中之重）**：仿射量化公式 `q = round(x/s) + z`，scale/zero-point 求解、对称 vs 非对称、KL散度校准（TensorRT 用的 Entropy 方法）、MSE 校准。数学不深，但必须能**手推一个 3×3 矩阵从 FP32 → INT8 → 反量化**的全过程。
- **信息论（概念级）**：熵、KL散度——理解量化校准和蒸馏的损失函数。
- **离散数学（轻量）**：图论基础——理解计算图（DAG）、拓扑排序、图优化 pass。

### 0.3 量化交易方向加修
- **时间序列分析**：ARIMA、GARCH（波动率建模）、自相关——策略研究的基本功。
- **随机过程（概念级）**：布朗运动、伊藤引理（了解即可）——理解期权定价类策略。
- **统计学习**：回归、正则化（L1/L2）、过拟合与样本外检验——量化策略的本质是"统计上显著"，不是"逻辑上正确"。
- **微积分/优化**：比 AI 方向稍深一点，理解组合优化（均值-方差）。

> ⚠️ 学习节奏：数学不必先学完再动手。建议与第一阶段的编程并行，遇到瓶颈时回头补。

---

## 第一部分：共同地基（两条路线共用的"下层"）

### 阶段 1：现代 C++ 与性能编程（约 2–3 个月）
1. **语言核心**：RAII、移动语义、智能指针、模板、Lambda、`constexpr`；C++17 的 `std::optional/variant/string_view`，C++20 的 concepts、coroutines（了解）。
   - 书：《Effective Modern C++》、《C++ Primer》（查漏）、《A Tour of C++》。
2. **并发编程**：`std::thread`、`std::mutex`、`std::atomic` 与**内存序**（relaxed/acquire/release/seq_cst）、条件变量、线程池。
   - 书：《C++ Concurrency in Action》。
3. **编译与工具链**：理解 `-O0/-O2/-O3` 差异、内联、LTO、静态/动态链接；会用 `gdb`、`valgrind`、`AddressSanitizer`。
4. **度量习惯养成**：学会写 micro-benchmark（Google Benchmark），建立"任何优化必须有数据"的习惯。

### 阶段 2：计算机体系结构（约 1.5–2 个月，与阶段1可并行）
1. **存储层级**：L1/L2/L3 缓存、缓存行（64B）、缓存未命中开销（~100ns 级）、伪共享（False Sharing）及 `alignas(64)` 避免方法、硬件预取。
2. **指令级**：SIMD（SSE/AVX2/AVX-512）、流水线、分支预测（为何 `std::sort` 有分支预测优化）、乱序执行。
3. **动手**：用 Intrinsics 手写一次 SIMD 求和/矩阵乘；用 `perf stat`（cache-misses、branch-misses）分析自己的程序。
   - 书：《计算机体系结构：量化研究方法》（当字典）、《CSAPP》第 3、5、6、9 章。

### 阶段 3：操作系统与 Linux（约 2 个月）
1. **进程/线程**：上下文切换开销（~1–2µs）、CPU 亲和性（`taskset`、`sched_setaffinity`）、NUMA（`numactl`、本地内存访问）。
2. **内存管理**：虚拟内存、页表、TLB、大页（HugePages）、`mmap`、内存分配器（ptmalloc/jemalloc/tcmalloc 的取舍）。
3. **I/O**：用户态/内核态切换成本、epoll、io_uring。
4. **实时性调优**：`chrt`（实时调度）、中断亲和性（`irqbalance` 关闭）、隔离核心（`isolcpus`）。
   - 书：《Linux内核设计与实现》、CSAPP 第 9 章；动手：写一个多线程 echo server，对比 epoll 与 io_uring。

### 阶段 4：网络与协议（约 1.5 个月）
1. **TCP/UDP 深入**：三次握手、Nagle、拥塞控制；为什么低延迟场景偏爱 UDP 组播。
2. **内核旁路概念**：DPDK（用户态轮询 PMD）、Solarflare OpenOnload、RDMA 零拷贝。
3. **时间同步**：NTP vs PTP（IEEE 1588），理解为何交易系统需要纳秒级同步。
   - 动手：写一个 UDP 组播收发程序并测延迟；用 `tcpdump`/Wireshark 抓包分析。

---

## 第二部分：AI推理端技术栈（自下而上）

### 阶段 5：先懂"训练端"——推理的前置知识（约 1 个月）
> 不要求会训大模型，但必须知道模型从哪来、计算图长什么样。

1. **深度学习框架基础**：用 PyTorch 训一个小型 CNN（MNIST/CIFAR-10），理解 Tensor、Autograd、`nn.Module`、`state_dict`。
2. **经典模型拓扑（见下表）**——它们是推理优化的"标准测试对象"。
3. **模型导出链路**：PyTorch → TorchScript / ONNX 的导出与验证。
4. **理解算子**：Conv2D（im2col/Winograd）、GEMM、LayerNorm、Softmax、Attention——推理优化的本质就是优化这些算子。

#### 经典模型例子与推理特点
| 模型 | 结构 | 推理端的意义/优化点 |
| :--- | :--- | :--- |
| **ResNet-50** | CNN + 残差连接 | 推理入门"果蝇"；考计算图融合（Conv+BN+ReLU）、INT8 PTQ 后精度几乎无损 |
| **YOLOv8** | 检测网络 | 动态/多尺度输入、NMS 后处理优化、端侧部署典型对象 |
| **BERT-base** | Transformer Encoder | 静态序列长度问题、Attention 算子融合、动态 shape、蒸馏后部署（DistilBERT） |
| **GPT-2 / LLaMA 系** | Decoder-only LLM | **KV Cache**、**预填充(Prefill) vs 解码(Decode)** 两阶段、内存带宽瓶颈、Continuous Batching、W4A16 量化 |
| **Stable Diffusion (UNet)** | 扩散模型 | 多步迭代推理、步数蒸馏（LCM/Turbo）、显存优化 |

### 阶段 6：硬件层——GPU 架构与 CUDA（约 1.5–2 个月）
1. **GPU 架构**：SM 结构、Warp 调度、显存层级（Register → Shared Memory → L2 → HBM Global Memory）、Tensor Core。
2. **CUDA 编程模型**：Grid/Block/Thread、memory coalescing（合并访存）、Shared Memory 分块（tiled GEMM）、CUDA Stream 与 CUDA Graphs。
3. **动手**：手写 SGEMM 并逐步优化（naive → shared memory tiling → 达到 cuBLAS 的 70%+ 即合格）。
   - 资源：《CUDA C++ Programming Guide》官方文档、PMPP 书（《Programming Massively Parallel Processors》）、NVIDIA DLI 免费课程。

### 阶段 7：模型压缩与量化（约 1 个月）
1. **量化**：PTQ（训练后量化）vs QAT（量化感知训练，插入伪量化节点 + straight-through estimator）；INT8 / FP8(E4M3) / FP4(E2M1) / W4A16；Per-Tensor vs Per-Channel vs Block-Wise（GPTQ、AWQ 属于这一类）；校准方法（Min-Max、Percentile、MSE、Entropy）。
2. **其他压缩**：结构化/非结构化剪枝、知识蒸馏、低秩分解。
3. **动手**：对 ResNet-50 做 TensorRT INT8 PTQ，记录精度-速度曲线；对一个小 LLM 做 AWQ/GPTQ W4A16 量化，对比 perplexity。

### 阶段 8：计算图与编译层（约 1.5 个月）
1. **图优化**：常量折叠、算子融合（Conv+BN+ReLU）、内存布局重排、死代码消除——理解推理引擎加载模型时到底做了什么。
2. **AI 编译器**：TVM（AutoTVM/Ansor 自动调优）、MLIR（方言、pass 机制）、XLA 概念。
3. **动手**：用 TVM 对一个模型 auto-tune，对比调优前后延迟；用 Netron 查看一个 ONNX 图，手工找出可融合的算子。

### 阶段 9：运行时与推理引擎（约 1.5 个月）
1. **推理引擎选型与原理**：
   - **TensorRT**：NVIDIA 旗舰，Builder → Engine → 反序列化执行，INT8/FP8 支持。
   - **ONNX Runtime**：跨平台，Execution Provider 机制（CUDA/TRT EP）。
   - **OpenVINO**：Intel CPU/集显。
   - **vLLM**：LLM 专用——PagedAttention（把操作系统虚拟内存分页思想用到 KV Cache）、Continuous Batching。
   - **llama.cpp**：CPU/边缘端，GGUF 格式、K-quants。
2. **服务化**：Triton Inference Server（动态 batching、多模型实例）、gRPC/REST 接口。
3. **系统级**：内存池与零拷贝、多流并行、动态 shape 的 padding/packing 策略、CPU 亲和性与 NUMA（GPU 机器同样适用）。
4. **动手**：把 BERT 部署成 Triton 服务，测 P99 延迟与吞吐的权衡；用 vLLM 起一个量化 LLaMA，观察 batching 对吞吐的影响。

### 阶段 10（高级）：阅读与贡献
- 精读 **llama.cpp** 的量化内核（`ggml` 目录）、**vLLM** 的 PagedAttention 实现、**ONNX Runtime** 的图优化 pass。
- 前沿跟进：FP8/FP4 训练推理、Mamba/线性 Attention、投机解码（Speculative Decoding）、MoE 推理调度。

---

## 第三部分：量化低延迟交易系统技术栈（自下而上）

### 阶段 5'：先懂"量化"是什么——业务前置知识（约 1 个月）
> 不知道为什么买为什么卖，就无法设计出对的系统。

1. **市场基础**：股票/期货/期权基础概念、限价单/市价单、订单簿（Order Book：BID/ASK 深度）、撮合机制（价格-时间优先）。
2. **市场微观结构**：Tick 数据、Level-1/Level-2 行情、盘口变化事件（增量行情）、滑点与冲击成本、做市商角色。
3. **策略类型谱系**（了解即可，帮助理解系统需求）：
   - 低频量化（因子选股、日频调仓）→ 延迟要求秒级，系统简单；
   - 中频 CTA/统计套利 → 延迟要求毫秒级；
   - **高频/超低延迟（做市、抢单、延迟套利）→ 微秒级，是本路线的目标场景**。
4. **回测概念**：事件驱动回测 vs 向量化回测、前视偏差、过拟合、样本外检验。
   - 书：《高频交易》(Irene Aldridge，概念级)、《Trading and Exchanges》(Larry Harris，微观结构圣经，可选择性阅读)。
   - 动手：用 Python（pandas + ccxt/akshare）写一个简单的双均线回测，亲历"数据 → 信号 → 模拟成交"的流程。

### 阶段 6'：极致性能编程（约 1.5 个月）
1. **无锁编程**：`std::atomic` 内存序在实践中的含义、无锁环形缓冲区（单生产者单消费者 SPSC 是热点标配）、`moodycamel::ConcurrentQueue`、RCU 概念、seqlock。
2. **内存纪律**：热点路径**禁止** `new/delete`；对象池、内存池、自定义 STL 分配器、栈上分配、预分配 Warmup。
3. **CPU 纪律**：核心绑定（`isolcpus` + `taskset`）、NUMA 本地分配、关闭超线程干扰、`chrt` 实时调度。
4. **分支与缓存友好**：分支预测友好写法（`[[likely]]`）、数据结构布局（SoA vs AoS）、`cachegrind` 分析。
5. **动手**：手写一个 SPSC 无锁环形队列 + 一个对象池，用 benchmark 证明其正确与快；实现一个简化版限价订单簿（价格桶 + 双向链表）。

### 阶段 7'：网络极致优化（约 1.5 个月）
1. **协议**：二进制行情协议解析（零拷贝、字段偏移读取）、FIX 协议、FAST/SBE 编码；手写一个 ITCH 5.0 解析器。
2. **内核旁路**：DPDK 环境（大页、UIO/VFIO、PMD 轮询模式）、OpenOnload（LD_PRELOAD 透明加速）、PF_RING；理解轮询 vs 中断的延迟/功耗权衡。
3. **RDMA**：Verbs API 概念、零拷贝发送、QP/内存注册。
4. **动手**：DPDK 收包 → 解析 UDP 组播行情 → 打时间戳，测端到端延迟分布（关注 P99.9）。

### 阶段 8'：系统级确定性保障（约 1 个月）
1. **消除抖动**：关闭 CPU 频率调节（performance governor）、C-states（节能状态会引入数十微秒唤醒延迟）、ASLR、THP；网卡中断合并（interrupt coalescing）权衡。
2. **时钟**：PTP（IEEE 1588）+ 硬件时间戳网卡；理解 NIC 硬件时间戳 vs 软件时间戳的差异；全链路事件打点。
3. **延迟测量方法论**：HDR Histogram、关注 P99/P99.9/P99.99 与 max 而非 mean；`perf`、LTTng、DPDK PDUMP/tracepoint。
4. **动手**：搭两台机器（或虚拟机）做 PTP 同步实验；给自己阶段 7' 的程序做完整延迟剖析报告。

### 阶段 9'：交易系统工程（约 2 个月）
1. **事件驱动架构**：行情 → 策略 → 风控 → 订单网关的单线程核心 + 多线程 I/O 的经典分层；线程间用 SPSC 队列通信。
2. **风控内嵌**：头寸限制、价格偏离检查、自成交防护（Self-Trade Prevention）、最大下单频率——微秒内完成。
3. **订单网关**：交易所接口（加密货币交易所 REST/WS，或 CTP/柜台接口）、断线重连、序列号恢复、预生成的订单模板。
4. **回测与模拟撮合**：事件驱动回测引擎（延迟建模：行情延迟 + 策略延迟 + 网关延迟）、撮合引擎模拟（订单簿撮合逻辑）。
5. **动手（毕业项目）**：构建一个完整的事件驱动回测 + 模拟撮合引擎：DPDK/普通 socket 收行情（可用加密货币实时行情替代）→ 订单簿维护 → 简单做市策略 → 模拟撮合 → 全链路微秒级时间戳统计。

### 阶段 10'（高级）
- 精读 **moodycamel::ConcurrentQueue**、**folly** 的 `MPMCQueue`/`ProducerConsumerQueue`、**Aeron** 的设计论文。
- 关注：FPGA 硬件加速交易（HLS 写撮合/行情解析）、交易所 colocation、FPGA vs 软件的延迟对比文献。

---

## 第四部分：开源项目推荐（学习计划末尾）

### 共同地基
| 项目 | 学什么 |
| :--- | :--- |
| **folly** (Facebook) | 现代C++工程典范：并发容器、内存分配、`ProducerConsumerQueue` |
| **moodycamel::ConcurrentQueue** | 无锁队列的最佳单文件实现，逐行精读 |
| **CSAPP / CMU 15-213 labs** | Cache Lab、Malloc Lab——地基中的地基 |

### AI 推理方向
| 项目 | 学什么 | 难度 |
| :--- | :--- | :--- |
| **llama.cpp** | CPU量化内核、GGUF、内存管理，代码量适中易读，适合贡献 | ⭐⭐ |
| **TensorRT-LLM** (NVIDIA) | LLM推理优化天花板：算子融合、FP8、inflight batching | ⭐⭐⭐⭐ |
| **vLLM** | PagedAttention、Continuous Batching，LLM 服务事实标准 | ⭐⭐⭐ |
| **ONNX Runtime** | 跨平台推理引擎架构、Execution Provider、图优化 pass | ⭐⭐⭐ |
| **TVM / Apache TVM** | AI编译器：auto-tuning、schedule、MLIR 前身思想 | ⭐⭐⭐⭐ |
| **NCNN / MNN / TFLite Micro** | 端侧推理：手写 ARM NEON 算子，理解极致内存优化 | ⭐⭐⭐ |
| **NVIDIA cutlass** | GEMM/Tensor Core 模板库——理解 GEMM 优化的极致 | ⭐⭐⭐⭐⭐ |

### 量化低延迟方向
| 项目 | 学什么 | 难度 |
| :--- | :--- | :--- |
| **DPDK** | 内核旁路标准实现，examples/ 目录是入门起点 | ⭐⭐⭐ |
| **Aeron** (Real-Logic) | 超低延迟消息传输，设计文档一流，金融界广泛使用 | ⭐⭐⭐ |
| **LMAX Disruptor** | 无锁事件驱动架构的经典，面试与设计必读 | ⭐⭐ |
| **libtrading** | 交易所协议(ITCH/FIX)的C实现，学习行情解析 | ⭐⭐ |
| **hftbacktest** | 事件驱动 HFT 回测框架（含延迟建模），Python/Rust | ⭐⭐ |
| **matching-engine 类项目**（如 ciroque/plasma、open-source matching engines） | 订单簿与撮合实现 | ⭐⭐⭐ |
| **Seastar** (ScyllaDB) | shared-nothing 单线程核心架构，NUMA 友好设计范本 | ⭐⭐⭐⭐ |
| **cyrus-d悬挂/hdrhistogram** (HdrHistogram) | 长尾延迟测量的标准工具 | ⭐ |

### 量化业务辅助
| 项目 | 学什么 |
| :--- | :--- |
| **vn.py** | 国内最流行的量化交易框架，理解事件引擎与网关设计 |
| **backtrader / vectorbt** | 回测框架两种范式（事件驱动 vs 向量化） |
| **qlib** (Microsoft) | AI量化研究平台，连接两个方向的桥梁 |
| **nautilus_trader** | 高性能 Rust/Cython 事件驱动交易框架，代码质量高 |

---

## 第五部分：时间线总览

| 阶段 | 时长 | 内容 | 里程碑产出 |
| :--- | :--- | :--- | :--- |
| 0–1 | 2–3 月 | 数学(并行) + 现代C++ | 无锁数据结构小作业 |
| 2–4 | 4–5 月 | 体系结构、OS、网络 | SIMD GEMM + perf 分析报告 |
| 5 | 1 月 | 训练端/量化概念（各自方向） | PyTorch 小模型 + 回测小项目 |
| 6–9 (AI) | 5–6 月 | CUDA → 量化 → 编译 → 引擎 | TensorRT INT8 部署 + LLM 量化报告 |
| 6'–9' (低延迟) | 5–6 月 | 无锁 → 网络 → 确定性 → 交易系统 | 全链路微秒级事件驱动模拟系统 |
| 10 | 持续 | 开源贡献 + 前沿论文 | 有被合并的 PR |

**核心心法**（保留原计划）：机械同理心（想象CPU执行你的代码）、度量不猜测（perf 是眼睛）、重造轮子（亲手实现内存池/无锁队列/算子）、确定性 > 平均速度（盯 P99.9）。
