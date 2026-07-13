# ImGui 集成中的 depth clip / depth clamp 补丁说明

> 本目录下的 `0001-sdlgpu3-enable-depth-clip.patch` 修改了 vendored 的 Dear ImGui
> SDL_GPU 后端。这份文档解释**它修了什么、为什么需要它、以及它会不会影响你的渲染**，
> 并帮助后来者避免重新踩坑或误删补丁。

---

## 0. 如果你是搜索报错来的（TL;DR）

程序启动时弹出断言：

```
Assertion failure at SDL_CreateGPUGraphicsPipeline_REAL (SDL_GPU.c:1152):
'!"Rasterizer state enable_depth_clip must be set to true (FEATURE_DEPTH_CLAMPING disabled)"'
```

**原因**：ImGui 的 SDL_GPU 后端把管线的 `enable_depth_clip` 硬编码成了 `false`，
而 FNA3D 在创建设备时关闭了 `depthClamp` 特性 —— 这两者不兼容。

**修复**：本目录的补丁把 ImGui 后端那一行改成 `enable_depth_clip = true`，
由 CMake 在配置阶段自动打上。你**不需要手动做任何事**，正常构建即可。

想理解这背后的图形学与 SDL 语义，继续往下看。

---

## 1. 背景知识：近/远平面、裁剪与钳制

### 1.1 视锥体与近/远平面

3D 场景经过相机投影后，只有落在**视锥体（view frustum）**内的东西才会被绘制。
视锥体在深度方向上由两个平面界定：

- **近平面（near plane）**：离相机最近的可见边界；
- **远平面（far plane）**：最远的可见边界。

顶点变换后，深度被映射到一个标准区间（在 SDL_GPU / Vulkan / D3D 里是 **[0, 1]**，
即 NDC 深度；对应视口的 `min_depth`/`max_depth`）。

### 1.2 几何体"戳出"深度范围时怎么办？

当一个三角形有一部分比近平面还近、或比远平面还远时，光栅化器有两种处理策略。
**这正是 `enable_depth_clip` 控制的东西**：

```
侧视图（相机朝 +Z 看），| 为近/远裁剪平面：

          near                        far
           |                           |
    ●------|---------- 场景几何 --------|------●
    A      |                           |      B
 (比近平面还近)                       (比远平面还远)

  depth clip  = true  →  A、B 所在的片元被【裁掉 / 丢弃】（标准行为）
  depth clamp = false →  A、B 的片元【保留】，深度被【钳制】到 [near, far] 端点
```

- **Depth clip（裁剪）**：超出 [near, far] 的片元直接**丢弃**。这是绝大多数渲染
  想要的默认行为。
- **Depth clamp（钳制）**：**关闭**近/远裁剪，把越界深度**夹**到区间端点，片元**照常画**。
  它需要 GPU 硬件的 `depthClamp` 特性支持。

### 1.3 SDL_GPU 里这个标志名字有点"反直觉"

`SDL_gpu.h` 中的定义（注意语义）：

```c
bool enable_depth_clip;  /**< true 开启 depth clip，false 开启 depth clamp。 */
```

也就是说 `false` **并不是**"不做深度处理"，而是"**改用 depth clamp**"。
很多人第一眼会误读成"关掉深度裁剪 = 无所谓"，其实它是在**主动请求钳制模式**，
而钳制模式是要硬件特性撑腰的。这是本次踩坑的语义根源之一。

---

## 2. 我们为什么会踩坑（根因）

三件事叠在一起：

### 2.1 FNA3D 主动关闭了 `depthClamp` 设备特性

`src/FNA3D_Driver_SDL.c`（设备创建处）：

```c
SDL_SetBooleanProperty(props, "SDL.gpu.device.create.feature.depth_clamping", false);
```

FNA3D 出于**可移植性**（能在部分旧的 / 移动端设备上跑）主动禁用了这个可选特性。
SDL 里该特性**默认是 ON 的**，是 FNA3D 特意把它关掉。

### 2.2 SDL 的硬性规定

`SDL_gpu.h` 文档明确写道：

> 当 `depth_clamping` 特性被禁用时，设备没有 depth clamp 支持，
> `SDL_GPURasterizerState.enable_depth_clip` **必须恒为 true**。

`SDL_GPU.c:1152` 的断言就是在强制执行这条规则。

### 2.3 ImGui 后端硬编码了 `false`

vendored 的 `imgui_impl_sdlgpu3.cpp` 在创建自己的图形管线时写死了：

```c
rasterizer_state.enable_depth_clip = false;   // 请求 depth clamp
```

这行代码**只在设备保留了 depthClamp 时才合法**（即 SDL 的默认情况）。一旦碰上
FNA3D 这种关掉了该特性的设备，就必然触发 §2.2 的断言 —— 且**每一个** FNA3D 设备都会中招。

> 一句话：ImGui 后端假设了"设备支持 clamp"，而 FNA3D 恰恰把 clamp 关了。

---

## 3. 修复方式与为什么用"补丁"而非直接改

补丁只做一处改动：

```diff
- rasterizer_state.enable_depth_clip = false;
+ rasterizer_state.enable_depth_clip = true;
```

### 为什么这样改是安全且正确的

ImGui 画的是 **2D 界面**：深度全在 [0,1] 内、**深度测试关闭、不写深度**、几何体
永不跨越近/远平面。对它而言，clip 与 clamp **行为完全等价**，改成 `true` 没有任何
视觉或功能差异，只是让它符合设备约束。

### 为什么用 tracked patch，而不是直接改 submodule 文件

`thirdparty/imgui` 是一个 **git submodule**，指向上游某个固定 commit。
如果直接改它的文件：

- 会让 submodule 处于 dirty 状态，语义不清；
- 一旦有人执行 `git submodule update`，改动会被**悄悄冲掉**，断言又回来了。

因此改动以**补丁文件**形式存放在本目录，由 `CMakeLists.txt` 在**配置阶段**自动、
**幂等**地打上（用 `git apply --reverse --check` 先判断是否已打过，避免重复应用）。
这样 submodule 仍停在原始 commit，补丁也能在 `submodule update` 之后自动重新生效。

> ⚠️ **不要手动删除本补丁或还原那一行**，否则断言会回归。若要彻底不需要它，
> 见 §5 —— 那才是正确的"关闭"方式。

---

## 4. 这个改动到底影响了什么？——基本为零

| 关注点 | 是否受影响 | 说明 |
|---|---|---|
| SSAO / gbuffer / 你自己的 effect | **否** | 这些管线我们一行没动，它们本来就是 `true` |
| ImGui 界面的外观 | **否** | 2D、深度测试关闭，clip 与 clamp 等价 |
| 项目原本可用的能力 | **否** | 见下 |

关键点：FNA3D **在设备级别已经全局关掉了 `depthClamp`**（§2.1）。也就是说，在当前
这个 FNA3D 构建里，**任何管线都无法使用 depth clamp**，ImGui 那个 `false` 从一开始
就是非法的、注定报错的写法。我们的补丁只是让 ImGui**回到这个既有约束之内**，
并没有拿走任何"本来能用"的功能。

---

## 5. 什么时候才真的需要 `false`（depth clamp）？

depth clamp 是个**专用特性**，普通不透明 / 半透明 / UI 渲染都用不到它。它主要出现在
深度相关的特殊 pass：

1. **阴影贴图（最典型）**：比光源近平面还近的遮挡物本应继续投影。用 clip 会把它裁掉，
   导致阴影出现空洞 / "peter-panning"。用 clamp 让这些片元以钳制后的深度照写，阴影才完整。
2. **模板阴影体（shadow volume）的封盖几何**：靠 clamp 规避远平面处的裁剪问题。
3. **刻意让几何体延伸出视锥**的体积 / decal 类技巧。

### 如果将来你确实要用 depth clamp，正确做法是：

**真正的开关不是本补丁**，而是设备创建时那行特性标志。步骤：

1. 在 `src/FNA3D_Driver_SDL.c` 把 depthClamp 重新打开：
   ```c
   // 由 false 改回 true（SDL 默认值）
   SDL_SetBooleanProperty(props, "SDL.gpu.device.create.feature.depth_clamping", true);
   ```
   ⚠️ 注意：这会牺牲对部分不支持 `depthClamp` 的旧 / 移动端设备的兼容性。
2. 确认目标设备确实支持该特性后，才可以在**特定管线**上把 `enable_depth_clip` 设成 `false`。
3. 此时本 ImGui 补丁仍应保留 `true`（ImGui 不需要 clamp），两者互不冲突。

在你没有做上述改动之前，**全项目保持 `enable_depth_clip = true` 是唯一合法且正确的选择**。

---

## 6. 防坑清单

- ✅ 遇到 `enable_depth_clip must be set to true` 断言 → 检查本补丁是否已生效
  （`CMake` 配置日志里应有 `Applied imgui patch: ...` 或 `imgui patch already applied`）。
- ✅ `git submodule update` 之后 → 重新运行 CMake 配置，补丁会自动重打，无需手动干预。
- ❌ 不要手动把 ImGui 后端那行改回 `false`，也不要删除本目录的补丁。
- ❌ 不要误以为"关掉 depth clip 无所谓" —— 在 SDL_GPU 里 `false` = 请求 clamp，是要硬件特性的。
- 🔧 真要用 depth clamp（如阴影贴图）→ 先改设备级 `depth_clamping` 特性（§5），别动 ImGui 补丁。
