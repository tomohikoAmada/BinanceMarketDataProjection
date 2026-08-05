# BinanceMarketDataProjection Milestone Plan

> **文档类型**：模块开发地图
> **模块**：BinanceMarketDataProjection
> **状态**：Proposed
> **版本**：0.1.0
> **日期**：2026-08-05
> **目标平台**：macOS Apple Silicon、Ubuntu Linux x86_64、Ubuntu Linux ARM64/RK3588
> **主要语言**：C++20
> **公共合同来源**：BinanceMarketDataContracts
> **部署方式**：第一阶段作为 Library 嵌入 Gateway 和 History，不作为独立服务运行

------

# 1. 模块目标

`BinanceMarketDataProjection` 是策略无关、确定性、可重放的市场状态投影库。

它接收已经规范化的 Binance 市场事件，维护本地市场状态，并生成版本化的：

- `LocalOrderBookSnapshot`
- `MarketStateSnapshot`
- `GapDescriptor`
- 相关 Projection 结果和状态

同一组输入事件，无论来自实时 Gateway 还是历史 Replay，都必须生成相同的结果。

------

# 2. 核心架构原则

## 2.1 逻辑独立、部署内嵌

Projection 是独立的领域模块和代码库，但第一阶段不作为独立进程运行。

主要嵌入方式：

```text
Gateway Runtime
    └── BinanceMarketDataProjection C++ Library

History / Replay
    └── BinanceMarketDataProjection C++ Library
        或 Python Binding
```

## 2.2 纯确定性

Projection Core 不得主动读取：

- 当前时间；
- 随机数；
- 网络；
- 文件系统；
- 环境变量；
- 全局配置；
- 线程状态。

所有影响结果的数据必须由调用方显式传入。

## 2.3 单写者

每个：

```text
Venue + Market + Symbol
```

对应一个 Projection 实例，由一个有序执行上下文写入。

Projection Core 内部：

- 不创建线程；
- 不使用线程池；
- 不回调消费者；
- 不负责并发调度；
- 不依赖全局可变状态。

## 2.4 Core 与 Wire 分离

核心算法不直接使用 Protobuf Message。

```text
Protobuf
   ↓
Protobuf Adapter
   ↓
Projection Domain Types
   ↓
Projection Core
```

## 2.5 精确数值

价格和数量的合同边界使用十进制字符串。

Projection 内部使用：

```text
PriceTicks   → int64 强类型
QuantityLots → int64 强类型
```

禁止在价格、数量和订单簿键中使用：

- `float`
- `double`
- `long double`
- `std::stod`
- 隐式舍入

## 2.6 策略无关

Projection 可以计算确定性市场状态，例如：

- Best Bid/Ask
- Mid Price
- Spread
- Microprice
- Top-N Depth
- Mark Price
- Index Price
- Funding Rate
- Open Interest

Projection 不得包含：

- Alpha；
- 预测；
- 策略信号；
- 训练参数；
- 买卖建议；
- 仓位管理；
- 风险决策。

------

# 3. 技术基线

## 3.1 正式依赖

第一阶段生产依赖控制为：

```text
C++20 Standard Library
Protocol Buffers C++
tl::expected 或项目内部 Expected 抽象
```

第一版订单簿容器：

```text
std::map
```

## 3.2 开发与测试依赖

```text
CMake
Ninja
Conan 2
GoogleTest
Google Benchmark
clang-format
clang-tidy
ASan
UBSan
TSan
libFuzzer
llvm-cov
```

后期可选：

```text
pybind11
Abseil btree_map
Boost.Container flat_map
Boost.Decimal test oracle
Boost.Multiprecision test oracle
```

## 3.3 明确不引入

Projection 第一阶段不引入：

- WebSocket Library；
- HTTP Client；
- gRPC Server Runtime；
- JSON Parser；
- Logging Framework；
- Database；
- Kafka；
- Redis；
- TBB；
- Folly；
- 撮合引擎；
- 内部线程池。

这些属于 Gateway、History、Health 或宿主应用的职责。

------

# 4. Milestone 总览

| Milestone | 名称                                    | 主要结果                            | 前置条件          |
| --------- | --------------------------------------- | ----------------------------------- | ----------------- |
| M0        | Repository Foundation                   | 可重复构建、测试和治理的 C++20 仓库 | Contracts 已合并  |
| M1        | Fixed-Point Numeric Core                | 精确价格和数量类型                  | M0                |
| M2        | Order Book Core                         | 无序号策略的订单簿状态核心          | M1                |
| M3        | Sequence and State Machine              | Spot/USD-M 序号、Gap、Resync        | M2                |
| M4        | Market State Projection                 | Best/Mid/Spread/Microprice/Top-N    | M3                |
| M5        | Protobuf Contract Adapter               | C++ 与 Contracts 双向适配           | M1–M4             |
| M6        | Determinism and Differential Validation | Live/Replay 一致性证明              | M3–M5             |
| M7        | Container and Performance Decision      | 容器选型与性能基线                  | M6                |
| M8        | Python Binding and History Integration  | Python Replay 复用 C++ Core         | M6–M7             |
| M9        | Gateway Embedding Interface             | Gateway 可直接链接 Projection       | M6–M7             |
| M10       | Platform Hardening                      | macOS/ARM64/RK3588 稳定性           | M8–M9             |
| M11       | Acceptance Candidate                    | 合同与模块进入稳定候选状态          | M10 + Gateway E2E |

------

# 5. M0：Repository Foundation

## 5.1 目标

建立一个不包含业务算法，但能够可靠构建、测试、分析和发布的 C++20 Library 仓库。

## 5.2 交付物

推荐仓库：

```text
tomohikoAmada/BinanceMarketDataProjection
```

推荐结构：

```text
BinanceMarketDataProjection/
├── CMakeLists.txt
├── CMakePresets.json
├── conanfile.py
├── conan.lock
├── README.md
├── AGENTS.md
├── ARCHITECTURE.md
├── CHANGELOG.md
├── LICENSE
├── cmake/
├── include/
├── src/
├── adapters/
├── tests/
├── fuzz/
├── benchmarks/
├── bindings/
└── docs/
    └── adr/
```

建立 CMake Targets：

```text
bmd_projection_core
bmd_projection_proto
bmd_projection_tests
bmd_projection_benchmarks
```

公共命名空间：

```cpp
binance_market_data::projection::v1
```

## 5.3 构建配置

至少提供：

```text
debug
release
asan
ubsan
tsan
coverage
benchmark
```

推荐命令：

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## 5.4 CI

初始 CI：

- Ubuntu x86_64 + GCC；
- Ubuntu x86_64 + Clang；
- macOS；
- Debug Build；
- Release Build；
- GoogleTest；
- clang-format；
- clang-tidy；
- ASan；
- UBSan。

ARM64/RK3588 在 M10 加入正式验收。

## 5.5 ADR

建立：

```text
ADR-0001-cpp20-projection-core.md
ADR-0002-fixed-point-internal-representation.md
ADR-0003-single-writer-order-book.md
ADR-0004-core-wire-separation.md
```

## 5.6 验收标准

- 全新 Clone 可以按文档构建；
- 构建过程不在 CMake Configure 阶段偷偷联网；
- Debug 和 Release 均成功；
- 测试 Target 可运行；
- ASan/UBSan 基础任务通过；
- 安装后可以被另一个 CMake 项目通过 `find_package` 或 `add_subdirectory` 使用；
- 公共头文件不泄露测试依赖；
- 工作树在构建后不产生未跟踪生成文件。

## 5.7 非目标

M0 不实现：

- Decimal Parser；
- Order Book；
- Sequence Policy；
- Protobuf Adapter；
- Python Binding。

------

# 6. M1：Fixed-Point Numeric Core

## 6.1 目标

建立跨平台、无二进制浮点误差的价格和数量基础。

## 6.2 核心类型

```cpp
class PriceTicks;
class QuantityLots;

struct SymbolSpec {
    std::string symbol;
    std::uint8_t price_scale;
    std::uint8_t quantity_scale;
    std::int64_t tick_size_units;
    std::int64_t step_size_units;
};
```

辅助类型：

```cpp
struct DecimalParseError;
struct DecimalFormatError;
struct SymbolSpecError;
```

## 6.3 功能

实现：

```cpp
Expected<PriceTicks, DecimalParseError>
parse_price(std::string_view, const SymbolSpec&);

Expected<QuantityLots, DecimalParseError>
parse_quantity(std::string_view, const SymbolSpec&);

std::string
format_price(PriceTicks, const SymbolSpec&);

std::string
format_quantity(QuantityLots, const SymbolSpec&);
```

需要处理：

- Scale；
- Tick Size；
- Step Size；
- 正负号；
- 前导零；
- 尾随零；
- 超出精度；
- int64 溢出；
- 中间计算溢出；
- 空字符串；
- 非法字符。

## 6.4 明确规则

禁止接受：

```text
NaN
Infinity
1e-5
+1.0
前后空格
多个小数点
负价格
负数量
超出允许 scale 的非零位
```

是否允许：

```text
1
1.0
1.000000
```

必须由合同语义明确，而不是由第三方 Parser 决定。

## 6.5 中间计算

Mid Price、Microprice 等中间运算应使用项目封装的宽整数能力：

```text
__int128
```

不能让 `__int128` 直接成为公共 API 类型。

## 6.6 测试

- 最小值；
- 最大值；
- int64 边界；
- Scale 0；
- 大 Scale；
- Tick Size 不整除；
- Step Size 不整除；
- Trailing Zero；
- 非法输入；
- Parse → Format；
- Format → Parse；
- 随机 Property Test；
- Decimal Parser Fuzz Test；
- 与高精度测试 Oracle 对比。

## 6.7 验收标准

- 所有有效 Contracts Fixture 数值可解析；
- 所有非法数值被明确拒绝；
- Parse/Format 循环稳定；
- macOS 与 Linux 输出一致；
- Debug/Release 输出一致；
- 不使用二进制浮点；
- ASan/UBSan 通过；
- Parser Fuzz 不崩溃；
- 热路径不抛出未捕获异常。

------

# 7. M2：Order Book Core

## 7.1 目标

建立与交易所连接和序号策略无关的订单簿状态容器。

## 7.2 第一版容器

```cpp
using BidLevels = std::map<
    PriceTicks,
    QuantityLots,
    std::greater<PriceTicks>
>;

using AskLevels = std::map<
    PriceTicks,
    QuantityLots,
    std::less<PriceTicks>
>;
```

容器类型不得泄露到公共 API。

## 7.3 核心操作

```cpp
class OrderBook {
public:
    ApplyLevelsResult apply_snapshot(
        std::span<const PriceLevel> bids,
        std::span<const PriceLevel> asks
    );

    ApplyLevelsResult apply_levels(
        std::span<const PriceLevel> bids,
        std::span<const PriceLevel> asks
    );

    std::optional<PriceLevel> best_bid() const;
    std::optional<PriceLevel> best_ask() const;

    std::vector<PriceLevel> top_bids(std::size_t limit) const;
    std::vector<PriceLevel> top_asks(std::size_t limit) const;

    void clear();
};
```

## 7.4 更新语义

对于价格档：

```text
quantity > 0 → 设置该价格档的绝对数量
quantity = 0 → 删除该价格档
```

不执行：

- 数量增量累加；
- 撮合；
- 成交生成；
- 订单队列维护。

## 7.5 Invariants

始终保证：

- Bid 严格按价格降序；
- Ask 严格按价格升序；
- Book 中不存在零数量档；
- 同一侧同一价格最多一个档位；
- Snapshot Apply 后旧状态被完整替换；
- 重复执行同一档位更新结果相同。

Crossed Book 不应被静默修复：

```text
best_bid >= best_ask
```

应产生显式状态或错误。

## 7.6 测试

- 空 Book；
- 单档；
- 多档；
- 插入；
- 更新；
- 删除；
- 删除不存在档；
- Snapshot 覆盖；
- 重复价格；
- Top-N；
- N=0；
- N 大于 Book；
- Crossed Book；
- 大 Snapshot；
- 大量连续删除；
- 随机更新 Property Test。

## 7.7 验收标准

- 所有 Invariant 有显式测试；
- 相同输入产生相同结果；
- 无网络和 Protobuf 依赖；
- 不创建线程；
- 不主动读取时间；
- 5000 档 Snapshot 可正确应用；
- ASan/UBSan 通过；
- 建立未优化的性能基线。

------

# 8. M3：Sequence Policy and Projection State Machine

## 8.1 目标

实现 Binance Spot 和 USD-M 的序号校验、同步状态、Gap 和 Resync 语义。

## 8.2 状态机

```cpp
enum class ProjectionState {
    Empty,
    SnapshotApplied,
    Synchronized,
    GapDetected,
    ResyncRequired
};
```

状态转换必须集中定义，不能分散在多个 `if` 中。

## 8.3 Apply 结果

```cpp
enum class ApplyStatus {
    Applied,
    AppliedAndSynchronized,
    IgnoredStale,
    Duplicate,
    GapDetected,
    ResyncRequired,
    RejectedInvalid
};

struct ApplyResult {
    ApplyStatus status;
    ProjectionState previous_state;
    ProjectionState current_state;
    std::optional<std::uint64_t> previous_update_id;
    std::optional<std::uint64_t> current_update_id;
    std::optional<GapDescriptor> gap;
};
```

## 8.4 Sequence Policy

定义独立策略：

```cpp
class SpotSequencePolicy;
class UsdMSequencePolicy;
```

输入至少包括：

```text
snapshot.lastUpdateId
U
u
pu（USD-M）
local last update ID
```

## 8.5 处理场景

- Snapshot 前收到 Diff；
- Snapshot 后的首次桥接；
- 正常连续更新；
- Stale；
- Duplicate；
- Overlap；
- Gap；
- `pu` 不匹配；
- Update ID 倒退；
- Gap 后继续收到事件；
- Reset；
- 新 Snapshot 恢复。

## 8.6 安全原则

一旦检测到无法证明连续性：

```text
ProjectionState = ResyncRequired
```

之后不得继续输出：

```text
book_synchronized = true
```

不得猜测、填补或静默忽略 Gap。

## 8.7 Spot 桥接风险

Spot 首次 Snapshot/Diff 桥接规则必须同时基于：

- Contracts 测试；
- Binance 官方语义；
- Recorder 实际数据；
- 明确的 ADR；
- 回归 Fixture。

在规则未完全确认前，相关行为保留为显式受控风险。

## 8.8 测试

Sequence State Machine 的每一个分支必须有显式测试：

- Spot 正常桥接；
- Spot Stale；
- Spot Gap；
- USD-M 正常 `pu`；
- USD-M `pu` 错误；
- Gap 后拒绝继续同步；
- 新 Snapshot 恢复；
- Duplicate；
- Overlapping Update；
- Reset。

## 8.9 验收标准

- 状态转换表完整；
- Spot 和 USD-M 策略分离；
- Gap 不会被静默忽略；
- Sequence/State Machine 的逻辑分支全部有明确测试；
- Gap 后必须通过新 Snapshot 才能恢复同步；
- 同一事件序列重复执行状态一致；
- Recorder 真实序列可重放。

完成 M3 后可以发布：

```text
0.1.0a1 — Projection Order Book Core Alpha
```

------

# 9. M4：Market State Projection

## 9.1 目标

基于同步订单簿生成策略无关的市场状态。

## 9.2 第一阶段输出

- Best Bid Price；
- Best Bid Quantity；
- Best Ask Price；
- Best Ask Quantity；
- Mid Price；
- Spread；
- Microprice；
- Top-N Bids；
- Top-N Asks；
- Source Book Update ID；
- Book Synchronized。

## 9.3 公式语义

### Mid Price

```text
(best_bid + best_ask) / 2
```

必须定义无法整除时的精确表示策略。

不得隐式转换为 `double`。

### Spread

```text
best_ask - best_bid
```

### Microprice

推荐语义：

```text
microprice =
(best_ask × best_bid_quantity
 + best_bid × best_ask_quantity)
/
(best_bid_quantity + best_ask_quantity)
```

必须明确：

- 分母为零时；
- 任一侧为空时；
- Crossed Book 时；
- 舍入规则；
- 输出 Scale；
- 中间整数溢出处理。

## 9.4 BookTicker Cross-check

允许输入 BookTicker 作为对照事实，但 BookTicker 不覆盖本地订单簿状态。

比较：

- Best Bid；
- Best Ask；
- Quantity；
- Update ID 可追踪性。

不一致时输出显式 Quality/Diagnostic，不自动修复本地 Book。

## 9.5 辅助市场数据

可在 M4 后半阶段加入：

- Mark Price；
- Index Price；
- Funding Rate；
- Next Funding Time；
- Open Interest。

这些数据使用最新状态覆盖，不参与订单簿连续性判断。

## 9.6 验收标准

- 输出字段语义与 Contracts 完全一致；
- 无同步订单簿时，不伪造 Best/Mid/Spread；
- Microprice 使用精确中间计算；
- Crossed Book 不输出正常同步状态；
- Top-N 顺序稳定；
- 相同状态生成相同 Snapshot；
- 不包含策略指标。

------

# 10. M5：Protobuf Contract Adapter

## 10.1 目标

让 C++ Projection 与 `BinanceMarketDataContracts` 的 Wire Contracts 正式互操作。

## 10.2 Contracts Pin

仓库记录：

```text
contracts.lock
```

示例：

```text
repository=tomohikoAmada/BinanceMarketDataContracts
commit=01d76a41929f36d89573159f5f458f9f1e378ada
version=0.2.0a1
```

CI 必须 Checkout 固定 Commit，不允许静默跟随 Contracts `main`。

## 10.3 C++ 代码生成

从 Contracts Canonical Proto 生成：

```text
.pb.h
.pb.cc
```

不得把修改过的 `.proto` 副本长期保存在 Projection 仓库。

## 10.4 Adapter

实现：

```cpp
Expected<projection::DepthUpdate, AdapterError>
depth_update_from_proto(...);

Expected<projection::DepthSnapshot, AdapterError>
depth_snapshot_from_proto(...);

Expected<projection::AggTrade, AdapterError>
agg_trade_from_proto(...);

Expected<projection::BookTicker, AdapterError>
book_ticker_from_proto(...);

Expected<projection_proto::LocalOrderBookSnapshot, AdapterError>
local_order_book_snapshot_to_proto(...);

Expected<projection_proto::MarketStateSnapshot, AdapterError>
market_state_snapshot_to_proto(...);
```

## 10.5 严格验证

Adapter 必须验证：

- Schema Version；
- Venue；
- Market；
- Symbol；
- Stream；
- Enum；
- Required Field；
- Optional Presence；
- Price/Quantity；
- Update ID；
- Source ID；
- Snapshot 状态；
- Top-N 顺序。

不得：

- 静默覆盖错误字段；
- 把 Unknown Enum 当作缺失；
- 使用 Proto 默认零值代替缺失；
- 忽略不兼容 Schema Version。

## 10.6 Cross-language Golden Test

```text
Python Contracts Fixture
    ↓
Python Protobuf Serialize
    ↓
C++ Parse
    ↓
Projection Apply
    ↓
C++ Snapshot Serialize
    ↓
Python Parse and Validate
```

## 10.7 验收标准

- C++ 可以解析 Contracts 生成的全部 Projection 相关 Fixture；
- C++ 输出可由 Python Contracts 验证；
- Decimal Trailing Zero 语义符合合同；
- Unknown Enum 和 UNSPECIFIED 明确报错；
- Proto Adapter 不泄露到 Core；
- Buf Breaking 检查继续由 Contracts 管理；
- Projection CI 能检测 Contracts Pin 漂移。

完成 M5 后可以发布：

```text
0.2.0a1 — Contract-integrated Projection Alpha
```

------

# 11. M6：Determinism and Differential Validation

## 11.1 目标

证明 Projection 在实时和历史上下文中具有相同语义。

## 11.2 测试输入

使用：

- Contracts Golden Fixture；
- Gateway Transcript Fixture；
- Recorder 真实 Raw Replay；
- Spot 数据；
- USD-M 数据；
- 正常流；
- Gap；
- Resync；
- Burst；
- Duplicate；
- Stale。

## 11.3 Differential Oracle

第一阶段可以使用 Recorder 当前 Python `LocalBookReconstructor` 作为参考实现。

同一输入分别运行：

```text
Recorder Python Reference
C++ Projection
```

每个 Checkpoint 比较：

- Last Update ID；
- State；
- Synchronized；
- Best Bid/Ask；
- Top-20；
- Top-100；
- Level Count；
- Gap；
- Source IDs。

参考实现与 C++ 结果不一致时，不自动认定任何一方正确，必须回到：

- 官方语义；
- Raw 数据；
- Contracts；
- ADR；

进行裁决。

## 11.4 Live/Replay 一致性

同一事件序列通过两种驱动方式：

```text
逐事件 Live Apply
批量 Replay Apply
```

最终状态必须相同。

从中间 Checkpoint 恢复后继续 Replay，最终状态也必须相同。

## 11.5 Deterministic Output

相同：

- Initial State；
- Input Events；
- SymbolSpec；
- Explicit Generated Time；

必须生成相同的：

- Domain Snapshot；
- Canonical JSON；
- Deterministic Protobuf Serialization。

## 11.6 验收标准

- Spot 与 USD-M 都有真实数据 Differential Test；
- 至少覆盖正常、Gap 和 Resync 场景；
- Live/Replay 最终状态一致；
- Checkpoint Restore 结果一致；
- 不依赖当前系统时间；
- 测试在多次运行间稳定；
- 不存在未解释的 Differential Difference。

------

# 12. M7：Container and Performance Decision

## 12.1 目标

使用真实数据决定是否继续使用 `std::map`，而不是提前优化。

## 12.2 候选实现

使用相同接口测试：

```text
A. int64 strong types + std::map
B. int64 strong types + absl::btree_map
C. int64 strong types + boost::container::flat_map
```

CNL 等数值库只作为独立 Numeric Spike，不与容器选择混在同一个结论中。

## 12.3 Benchmark 场景

```text
ApplySingleBidUpdate
ApplySingleAskUpdate
ApplyDepthBatch10
ApplyDepthBatch100
DeleteLevel
Top20Snapshot
Top100Snapshot
InitialSnapshot1000
InitialSnapshot5000
RecordedNormalReplay
RecordedBurstReplay
RecordedDeletionHeavyReplay
GapAndReset
```

## 12.4 指标

记录：

- p50；
- p95；
- p99；
- max；
- updates/s；
- batches/s；
- Snapshot Time；
- Allocation Count；
- RSS；
- Binary Size；
- Build Time；
- CPU 使用；
- Debug/Release 差异。

## 12.5 测试平台

至少：

```text
MacBook Air M3
Ubuntu x86_64
Ubuntu ARM64/RK3588
```

## 12.6 决策规则

只有候选容器同时满足：

1. 所有正确性测试通过；
2. 所有 Differential Test 一致；
3. 真实 Replay 中有稳定优势；
4. p99 或内存有明显改善；
5. 没有不可接受的构建和 ABI 风险；

才替换 `std::map`。

性能差异较小时，优先选择：

```text
实现更简单
依赖更少
可调试性更高
平台兼容性更好
```

## 12.7 输出

新增 ADR：

```text
ADR-0005-order-book-container-selection.md
```

记录：

- 测试数据；
- 硬件；
- 编译器；
- 编译参数；
- 候选；
- 结果；
- 最终选择；
- 重新评估条件。

## 12.8 验收标准

- Benchmark 可重复；
- 输入数据版本固定；
- 输出 JSON 可归档；
- 不使用单次最好结果；
- 容器决策有 ADR；
- 没有为 Microbenchmark 牺牲合同正确性。

------

# 13. M8：Python Binding and History Integration

## 13.1 目标

让 Python History、Replay 和测试系统复用同一个 C++ Projection Core。

## 13.2 技术

使用：

```text
pybind11
```

生成：

```text
binance_market_data_projection
```

Python Extension。

## 13.3 Binding API

绑定高层批量操作：

```python
projection.apply_snapshot(snapshot)
projection.apply_depth_update(update)
projection.apply_depth_updates(batch)
projection.snapshot_book(depth_limit=20)
projection.snapshot_market_state(depth_limit=20)
projection.reset(reason)
```

禁止为每个 Price Level 单独进行 Python/C++ 跨边界调用。

## 13.4 GIL

批量 Apply 和 Replay 操作应在安全情况下释放 GIL。

对象返回 Python 前必须已成为独立值，不暴露指向可变 C++ Order Book 的悬空引用。

## 13.5 Python Wheel

后续构建：

- macOS ARM64；
- Linux x86_64；
- Linux ARM64。

第一阶段允许只提供源码构建，不必立即建立完整公开 Wheel 发布体系。

## 13.6 验收标准

- Python 可以运行完整 Recorder Replay；
- Python/C++ Binding 不复制算法；
- Batch API 存在；
- GIL 策略有测试；
- 错误映射明确；
- C++ 生命周期不会造成悬空对象；
- Python Binding 输出与原生 C++ 输出相同。

------

# 14. M9：Gateway Embedding Interface

## 14.1 目标

让未来的 C++ Gateway 可以直接、低开销地嵌入 Projection。

## 14.2 Public Interface

提供稳定 CMake Target：

```text
BinanceMarketDataProjection::Core
BinanceMarketDataProjection::Proto
```

Gateway 可以：

```cmake
target_link_libraries(
    gateway
    PRIVATE
    BinanceMarketDataProjection::Core
    BinanceMarketDataProjection::Proto
)
```

## 14.3 Gateway 调用模型

```text
Gateway Receive/Parse
        ↓
Projection Adapter
        ↓
ProjectionEngine.apply(...)
        ↓
Immutable Snapshot
        ↓
Gateway Fanout
```

## 14.4 所有权

每个市场一个 Projection 实例：

```text
BTCUSDT Spot
BTCUSDT USD-M
```

由 Gateway 生命周期管理。

Projection 不持有：

- Gateway Consumer；
- gRPC Context；
- Socket；
- Queue；
- Logger；
- Metrics Registry。

## 14.5 时间注入

Snapshot 的：

- Generated UTC Time；
- Generated Monotonic Time；
- Source Receive Time；

由 Gateway 显式传入。

Projection 不自行调用系统 Clock。

## 14.6 Snapshot 策略

Projection 提供：

- Top-N Snapshot；
- Full Internal Checkpoint；
- Market State Snapshot。

不能让 Gateway Consumer 直接读取可变内部 Map。

## 14.7 集成测试

使用不连接 Binance 的 Gateway Harness：

```text
Recorded Proto Events
    ↓
Fake Gateway Driver
    ↓
Projection
    ↓
Gateway Stream Item
```

验证：

- Initial Snapshot；
- Diff；
- Gap；
- Resync；
- State Snapshot；
- Source Update ID；
- Generated Time；
- Consumer Stream 顺序。

## 14.8 验收标准

- Gateway 无需 FFI 即可链接；
- Projection Core 不依赖 Gateway；
- 单写者约束明确；
- Snapshot 返回值不暴露内部可变状态；
- Gateway Harness E2E 通过；
- Projection 不进入 gRPC Consumer Queue 管理。

完成 M9 后可以发布：

```text
0.3.0a1 — Gateway-embeddable Projection Alpha
```

------

# 15. M10：Platform Hardening

## 15.1 目标

证明 Projection 在目标设备和长期 Replay 下稳定运行。

## 15.2 平台矩阵

```text
macOS Apple Silicon / AppleClang
Ubuntu x86_64 / GCC
Ubuntu x86_64 / Clang
Ubuntu ARM64 / GCC
Ubuntu ARM64 / Clang
RK3588 / Ubuntu ARM64
```

## 15.3 Sanitizer

- ASan；
- UBSan；
- TSan；
- Leak 检查；
- Debug Iterator 可选。

Projection Core 是单写者，但 Gateway Harness 应加入 TSan 验证调用边界。

## 15.4 Fuzzing

持续 Fuzz：

- Decimal Parser；
- Price Formatter；
- Depth Update Adapter；
- Sequence Policy；
- Snapshot Apply；
- Proto Parse；
- State Transition。

建立最小 Corpus，并保存所有回归样例。

## 15.5 长期 Replay

至少执行：

```text
1 小时
24 小时
72 小时
168 小时
```

的真实 Recorder 数据 Replay。

观察：

- 内存是否持续增长；
- 最终状态是否稳定；
- 是否出现未解释 Gap；
- Snapshot 成本；
- Replay Throughput；
- Debug/Release 一致性；
- Restart/Checkpoint 一致性。

## 15.6 性能回归门

CI 中保存基准，但避免因普通云主机波动导致频繁误报。

正式性能门应在固定机器执行：

```text
Mac M3
RK3588
```

性能回归超过受控阈值时才阻止发布，阈值必须基于历史数据建立。

## 15.7 验收标准

- 所有目标平台构建成功；
- ASan/UBSan 无错误；
- TSan 集成场景无数据竞争；
- Fuzz 无未修复崩溃；
- 72h Replay 无内存持续增长；
- 168h Replay 结果可复现；
- M3 与 RK3588 均有归档 Benchmark；
- 安装和卸载过程不修改系统全局配置。

------

# 16. M11：Acceptance Candidate

## 16.1 目标

使 Projection 模块和相关合同具备进入稳定状态的证据。

## 16.2 前置条件

必须已经完成：

- C++ Projection Core；
- Spot/USD-M；
- Protobuf Adapter；
- Python/History Replay；
- Gateway Embedding；
- Cross-language E2E；
- 性能基线；
- ARM64/RK3588；
- 长期 Replay；
- Gap/Resync；
- Contracts Compatibility。

## 16.3 Contracts Acceptance

评估是否将以下合同从 `PROPOSED` 提升为 `ACCEPTED`：

- `local-order-book-snapshot.v1`
- `market-state-snapshot.v1`
- `gap-descriptor.v1`
- 相关 Market Events

提升合同状态必须单独 PR，并提供：

- C++ 实现证据；
- Python 验证；
- Gateway 使用；
- History 使用；
- Cross-language Fixture；
- Breaking Check；
- 语义说明；
- 迁移规则。

## 16.4 发布候选

建议版本：

```text
0.9.0 — Acceptance Candidate
```

满足稳定性后：

```text
1.0.0
```

`1.0.0` 不代表整个 BinanceMarketData 系统生产完成，只代表 Projection Library 的公共行为达到稳定基线。

## 16.5 验收标准

- 没有开放的核心正确性问题；
- Sequence Policy 已有真实数据证据；
- Live/Replay 一致；
- Gateway/History 使用相同 Core；
- 合同语义明确；
- Breaking Policy 明确；
- 性能和内存有基准；
- 长期运行通过；
- 已知限制完整记录。

------

# 17. 每个 Milestone 的统一质量门

每个 Milestone PR 至少执行：

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

cmake --preset release
cmake --build --preset release
ctest --preset release

cmake --preset asan
cmake --build --preset asan
ctest --preset asan

cmake --preset ubsan
cmake --build --preset ubsan
ctest --preset ubsan
```

以及：

```text
clang-format
clang-tidy
CMake configure test
CMake install test
Downstream consumer test
Public header self-containment test
```

涉及 Protobuf 时增加：

```text
Contracts Pin Check
C++ Proto Generation
Golden Fixture
Cross-language Roundtrip
```

涉及性能时增加：

```text
Google Benchmark
固定输入数据
机器和编译器信息
JSON 结果归档
```

------

# 18. Definition of Done

一个 Milestone 只有同时满足以下条件才算完成：

1. 代码已进入独立 Feature Branch；
2. 所有交付物存在；
3. 所有验收标准通过；
4. CI 全绿；
5. Sanitizer 通过；
6. 文档与代码一致；
7. 新公共语义有测试；
8. 重大决策有 ADR；
9. 没有通过 Skip、Ignore 或关闭检查掩盖问题；
10. PR 经审查后合并；
11. `main` 工作树可重复构建；
12. 剩余风险被明确记录。

“代码已经写完”不等于 Milestone 完成。

------

# 19. 分支与 PR 策略

每个 Milestone 使用独立分支，例如：

```text
feat/m0-repository-foundation
feat/m1-fixed-point-core
feat/m2-order-book-core
feat/m3-sequence-state-machine
feat/m4-market-state
feat/m5-protobuf-adapter
```

原则：

- 一个 PR 只处理一个主要里程碑；
- 不在同一个 PR 同时做核心算法和容器优化；
- 不在同一个 PR 同时修改 Contracts 和实现；
- Contracts 变化先在 Contracts 仓库独立评审；
- Feature Branch 禁止 Force Push，除非尚未公开且有明确理由；
- 大型 PR 使用 Draft；
- 合并前所有 CI 必须完成。

------

# 20. 风险登记

## R-P001：Spot 首次桥接语义

状态：开放。

控制措施：

- 官方语义核查；
- Recorder 实际数据；
- Contracts Fixture；
- 单独 ADR；
- 不确定时 Fail Closed。

## R-P002：Fixed-Point 溢出

控制措施：

- Checked Parser；
- 宽整数中间计算；
- UBSan；
- Property/Fuzz；
- 边界 Fixture。

## R-P003：通用容器性能不足

控制措施：

- `std::map` 先作为正确性基线；
- 真实 Replay Benchmark；
- Abseil/Boost 候选；
- 容器不泄露到 API。

## R-P004：C++ 与 Python 语义漂移

控制措施：

- Cross-language Fixture；
- Differential Test；
- 同一 Contracts Pin；
- Canonical Snapshot 比较。

## R-P005：Gateway 与 History 使用不同逻辑

控制措施：

- 两者链接同一个 Projection Core；
- 不复制算法；
- Live/Replay E2E；
- ADR-0006 验收。

## R-P006：过早优化

控制措施：

- M0–M6 以正确性为先；
- M7 才允许容器决策；
- 所有优化必须通过相同 Golden Test。

## R-P007：依赖版本和 ABI

控制措施：

- Conan Lockfile；
- Abseil 仅使用固定 LTS；
- 第三方容器不进入公共头文件；
- Projection 与 Gateway 源码级统一构建。

------

# 21. 明确非目标

Projection 模块不开发：

- Binance WebSocket Client；
- Binance REST Client；
- Snapshot 下载；
- Diff 缓冲和网络桥接；
- gRPC Server；
- Slow Consumer Queue；
- Recorder；
- Raw Storage；
- SQLite；
- Parquet；
- Archive；
- HTTP API；
- Browser UI；
- Strategy；
- Feature Engineering；
- Risk；
- Execution；
- Matching Engine。

如果某项功能需要网络、持久化、消费者调度或策略假设，默认不属于 Projection Core。

------

# 22. 推荐执行顺序

```text
M0 Repository Foundation
        ↓
M1 Fixed-Point Numeric Core
        ↓
M2 Order Book Core
        ↓
M3 Sequence and State Machine
        ↓
M4 Market State Projection
        ↓
M5 Protobuf Contract Adapter
        ↓
M6 Determinism and Differential Validation
        ↓
M7 Container and Performance Decision
        ├── M8 Python/History Integration
        └── M9 Gateway Embedding
                 ↓
        M10 Platform Hardening
                 ↓
        M11 Acceptance Candidate
```

M8 和 M9 可以在 M7 以后部分并行，但必须共享同一个已验证的 Projection Core。

------

# 23. 第一阶段开发范围

当前立即开始的开发范围应限制为：

```text
M0 + M1
```

第一个 PR：

```text
feat/m0-repository-foundation
```

第二个 PR：

```text
feat/m1-fixed-point-core
```

不要在仓库初始化 PR 中同时实现订单簿、Protobuf、Python Binding 或 Gateway 接口。

完成 M1 后再开始 M2，以免构建、数值和领域逻辑同时失控。

------

# 24. 模块完成定义

`BinanceMarketDataProjection` 只有在以下条件全部满足后，才能称为第一版完成：

- Spot 和 USD-M 订单簿正确；
- Gap 和 Resync 语义明确；
- 数值无二进制浮点误差；
- Live 与 Replay 一致；
- Gateway 与 History 使用相同 Core；
- Protobuf 双向适配通过；
- Python/C++ Cross-language 验证通过；
- M3 和 RK3588 运行通过；
- 长期 Replay 通过；
- 主要性能有基准；
- 相关合同具备 Acceptance 证据；
- 所有已知限制被记录。

在此之前，版本应明确标记为 Alpha、Developer Preview 或 Acceptance Candidate。