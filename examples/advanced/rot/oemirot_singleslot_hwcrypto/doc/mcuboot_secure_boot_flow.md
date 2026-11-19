# MCUboot 安全启动与升级验证流程

本文按当前 `oemirot_singleslot_hwcrypto` 配置梳理 MCUboot 的启动、镜像验证和 overwrite 更新流程。每张流程图同时提供 Mermaid 源码和 PNG 图片。

## 当前配置与结论

当前工程在 [mcuboot_config.h](../NUCLEO-G564RE/oemirot/mcuboot_config.h) 中启用了 `MCUBOOT_PRIMARY_ONLY`、`MCUBOOT_SINGLE_APPLICATION_SLOT`、`MCUBOOT_OVERWRITE_ONLY`、`MCUBOOT_VALIDATE_PRIMARY_SLOT`、`MCUBOOT_USE_HASH_REF` 和 `MCUBOOT_HW_ROLLBACK_PROT`。工程使用 EC384 签名、EC256 加密、AES-256 和 PSA Crypto；`MCUBOOT_DIRECT_XIP`、`MCUBOOT_RAM_LOAD` 未启用，因此编译的是普通 overwrite/swap 分支中的 `context_boot_go()`，不是 Direct-XIP/RAM-load 分支。

**当前源码存在单槽配置与 loader 行为不一致的问题。** `MCUBOOT_PRIMARY_ONLY` 让 `flash_area_id_from_multi_image_slot()` 不再映射 secondary slot，但 `BOOT_NUM_SLOTS` 仍为 2，普通 `context_boot_go()` 仍按两个 slot 遍历并打开 area。slot 1 的 area ID 无法映射，`flash_area_open()` 失败，loader 随后进入断言/`FIH_PANIC` 路径。也就是说，按当前代码，流程不能到达正常的 primary 验证和启动阶段。

`MCUBOOT_OVERWRITE_ONLY` 只选择“把 secondary 镜像覆盖复制到 primary”的安装方式；它本身不会令 loader 变成单槽，也不会跳过 secondary area。例程没有提供 secondary app 更新镜像时，这个 loader/flash-map 冲突仍需先解决。

## 1. `main()` 启动主流程

入口为 [platform_common/main.c](../../../../../middleware/mcuboot/platform_common/main.c#L96)。平台初始化的关键步骤见目标平台 [boot_hal.c](../NUCLEO-G564RE/oemirot/boot_hal.c#L409)。

```mermaid
flowchart TD
    A["复位进入 main()"] --> B["boot_platform_init()"]
    B --> C{"平台初始化成功？"}
    C -- 否 --> E["平台错误处理 / 停止启动"]
    C -- 是 --> D{"启用 fast-wakeup？"}
    D -- 是 --> W["读取已保存的应用向量并快速跳转"]
    D -- 否 --> F["初始化 mbedTLS 静态内存分配器"]
    F --> G["初始化 NV security counter"]
    G --> H["初始化 PSA Crypto"]
    H --> I["FIH_CALL(boot_go)"]
    I --> J{"boot_go 成功？"}
    J -- 否 --> K{"启用外部 loader？"}
    K -- 是 --> L["返回 no-image 状态"]
    K -- 否 --> M["FIH_PANIC / 不启动未验证镜像"]
    J -- 是 --> N["do_boot(boot_rsp)"]
    N --> O["计算应用向量地址与 flash device base"]
    O --> P["boot_platform_quit()"]
    P --> Q["保存 hash reference、应用最终保护"]
    Q --> R["清理 I-Cache 并经 RSS 跳转到应用"]
```

![MCUboot main 启动流程](mcuboot-main-flow.png)

`fast-wakeup` 是条件编译路径；当前配置未启用时走完整初始化和 `boot_go()`。平台初始化包括时钟/系统、RNG、PUF、flash、运行时与静态保护，以及读取和验证 HDPL1 OBK 配置、加载 hash references。`boot_platform_quit()` 保存 hash references 并完成跳转前的平台保护操作。

## 2. 当前编译的 `context_boot_go()` 主流程

普通分支位于 [loader.c](../../../../../middleware/mcuboot/boot/bootutil/src/loader.c#L2148)。另一个 `context_boot_go()` 位于同文件 Direct-XIP/RAM-load 条件分支；当前工程不使用它。

```mermaid
flowchart TD
    A["boot_go() 调用普通 context_boot_go()"] --> B["初始化各 image 的 slot、sector 与状态数据"]
    B --> C["遍历 image"]
    C --> D["遍历 slot，BOOT_NUM_SLOTS = 2"]
    D --> E["flash_area_id_from_multi_image_slot(slot, image)"]
    E --> F{"area ID 可映射？"}
    F -- 否：当前 primary-only 的 slot 1 --> G["得到无效 ID"]
    G --> H["flash_area_open() 失败"]
    H --> I["assert / FIH_PANIC：启动失败"]
    F -- 是：双槽布局 --> J["打开 slot area 并读取 sector/header"]
    J --> K["boot_prepare_image_for_update()"]
    K --> L{"发现待安装的更新？"}
    L -- 是 --> M["验证 secondary 候选镜像"]
    M --> N{"候选镜像有效？"}
    N -- 否 --> O["拒绝/擦除无效候选，继续处理 primary"]
    N -- 是 --> P["overwrite：secondary 复制覆盖 primary"]
    L -- 否 --> Q["保留现有 primary"]
    O --> R["MCUBOOT_VALIDATE_PRIMARY_SLOT：验证 primary"]
    P --> R
    Q --> R
    R --> S{"primary 验证通过？"}
    S -- 否 --> T["无可启动镜像 / FIH 错误"]
    S -- 是 --> U["更新/确认 anti-rollback counter"]
    U --> V["填充 boot_rsp，关闭 areas 并返回 boot_go"]
```

![普通 context_boot_go 流程](mcuboot-context-boot-go.png)

图中从“slot 1 无法映射”到 panic 是当前单槽配置的实际阻断点。后续的双槽更新及 primary 验证仅描述 loader 在 area 映射可用时的预期流程，不代表当前单槽工程能执行到这些步骤。

相关实现：

- `BOOT_NUM_SLOTS` 定义仍为 2：[bootutil_priv.h](../../../../../middleware/mcuboot/boot/bootutil/src/bootutil_priv.h#L56)。
- primary-only 下 secondary slot 映射被排除：[flash_map_extended.c](../../../../../middleware/mcuboot/platform_common/flash_map_extended.c#L56)。
- `flash_area_open()` 查不到 area 时返回错误：[flash_map.c](../../../../../middleware/mcuboot/platform_common/flash_map.c#L188)。
- flash area ID/大小来自目标 [flash_layout.h](../NUCLEO-G564RE/oemirot/flash_layout.h#L130)；secondary app slot 配置为空。

## 3. 更新候选识别与验证

`boot_prepare_image_for_update()` 读取 slot 的 sector/header 信息并检查 slot 兼容性，然后结合 trailer 状态判断是否有更新请求。若有候选更新，`boot_validated_swap_type()` 通过 trailer 状态识别操作类型；候选镜像必须先经过 `boot_validate_slot()`。在 overwrite 配置下，验证通过后才允许把 secondary 镜像复制到 primary。

```mermaid
flowchart TD
    A["boot_prepare_image_for_update()"] --> B["读取 sectors 与 image headers"]
    B --> C{"slot/header/区域约束有效？"}
    C -- 否 --> X["返回错误或无可启动镜像"]
    C -- 是 --> D["读取 primary/secondary trailer 状态"]
    D --> E["boot_validated_swap_type()"]
    E --> F{"检测到待安装状态？"}
    F -- 否 --> G["无更新：保留 primary"]
    F -- 是 --> H["boot_validate_slot(secondary)"]
    H --> I{"镜像验证成功？"}
    I -- 否 --> J["拒绝并清理无效候选"]
    I -- 是 --> K["返回有效更新类型"]
    K --> L["MCUBOOT_OVERWRITE_ONLY：允许复制安装"]
```

![更新候选识别与验证流程](mcuboot-update-candidate.png)

## 4. `boot_validate_slot()` 与镜像安全验证

`boot_validate_slot()` 先检查镜像 header、slot 大小及平台 hook，再进入 `boot_image_check()` / `bootutil_img_validate()`。当前配置启用了加密镜像、签名验证、hash reference 和硬件 rollback protection。

```mermaid
flowchart TD
    A["boot_validate_slot(slot)"] --> B["检查 header、镜像尺寸、slot 限制与 hook"]
    B --> C{"header/slot 合法？"}
    C -- 否 --> X["FIH_FAILURE"]
    C -- 是 --> D{"镜像是否需要解密？"}
    D -- 是 --> E["加载/准备解密密钥"]
    D -- 否 --> F["bootutil_img_validate()"]
    E --> F
    F --> G["计算镜像 hash：header + payload + protected TLV"]
    G --> H["校验 TLV 结构与 protected hash"]
    H --> I{"hash/TLV 有效？"}
    I -- 否 --> X
    I -- 是 --> J{"primary 的 hash reference 命中？"}
    J -- 是 --> K["跳过签名运算：可信 reference 与镜像 hash 匹配"]
    J -- 否 --> L["解析签名 key ID 并验证 EC384 签名"]
    L --> M{"签名有效？"}
    M -- 否 --> X
    M -- 是 --> N["按需更新 primary hash reference"]
    K --> O{"启用 HW rollback protection？"}
    N --> O
    O -- 是 --> P["读取镜像 security-counter TLV 与 NV counter"]
    P --> Q{"镜像 counter >= NV counter？"}
    Q -- 否 --> X
    Q -- 是 --> R["镜像验证成功"]
    O -- 否 --> R
```

![镜像安全验证流程](mcuboot-image-validation.png)

镜像 hash 覆盖 header、payload 和 protected TLV；加密 payload 在参与 hash 前会按配置解密。hash reference 只在 primary slot 的匹配路径起到避免重复签名计算的作用；reference 不匹配或不存在时仍需验证签名。硬件 rollback 检查要求镜像 counter 不低于 NV counter。counter 的初始化/读取/更新由 MCUboot security counter wrapper 调用平台接口，目标平台将值存放在 OBK NV counter 中。

## 5. Overwrite 安装与安装后复验

overwrite 更新并非 swap：`boot_perform_update()` 最终调用 `boot_copy_image()`，把已验证的 secondary 镜像复制到 primary；随后 `MCUBOOT_VALIDATE_PRIMARY_SLOT` 要求再次验证 primary。仅在安装及复验成功后，loader 才准备最终启动响应。

```mermaid
flowchart TD
    A["secondary 候选验证通过"] --> B["boot_perform_update()"]
    B --> C{"MCUBOOT_OVERWRITE_ONLY？"}
    C -- 是 --> D["boot_copy_image(secondary, primary)"]
    C -- 否 --> E["其他安装模式"]
    D --> F["擦除 primary 目标区域"]
    F --> G["逐段复制镜像；必要时解密 payload"]
    G --> H{"读写/复制成功？"}
    H -- 否 --> X["安装失败，不启动候选镜像"]
    H -- 是 --> I["安装完成并更新相关状态"]
    I --> J["MCUBOOT_VALIDATE_PRIMARY_SLOT"]
    J --> K["重新验证 primary 的 hash、签名/reference 与 rollback counter"]
    K --> L{"primary 复验通过？"}
    L -- 否 --> X
    L -- 是 --> M["更新 NV security counter"]
    M --> N["填充 boot_rsp 并返回 main()"]
```

![Overwrite 安装与安装后复验流程](mcuboot-overwrite-installation.png)

## 6. 主要源码索引

- 入口与跳转：[main.c](../../../../../middleware/mcuboot/platform_common/main.c#L96)、目标平台 [boot_hal.c](../NUCLEO-G564RE/oemirot/boot_hal.c#L409)。
- `boot_go()` 与普通 `context_boot_go()`：[loader.c](../../../../../middleware/mcuboot/boot/bootutil/src/loader.c#L2148)、Direct-XIP/RAM-load 分支 [loader.c](../../../../../middleware/mcuboot/boot/bootutil/src/loader.c#L2589)。
- 更新判定、slot 验证、复制和安装：[loader.c](../../../../../middleware/mcuboot/boot/bootutil/src/loader.c#L1014)、[loader.c](../../../../../middleware/mcuboot/boot/bootutil/src/loader.c#L1394)、[loader.c](../../../../../middleware/mcuboot/boot/bootutil/src/loader.c#L1676)、[loader.c](../../../../../middleware/mcuboot/boot/bootutil/src/loader.c#L1875)。
- hash、TLV、签名与 rollback 检查：[image_validate.c](../../../../../middleware/mcuboot/boot/bootutil/src/image_validate.c#L83)、[image_validate.c](../../../../../middleware/mcuboot/boot/bootutil/src/image_validate.c#L450)、[image_validate.c](../../../../../middleware/mcuboot/boot/bootutil/src/image_validate.c#L677)。
- NV counter wrapper 与目标平台实现：[security_cnt.c](../../../../../middleware/mcuboot/platform_common/security_cnt.c#L29)、[boot_nv_counters.c](../NUCLEO-G564RE/oemirot/boot_nv_counters.c#L39)。
- hash-reference 的平台存取：[boot_hal_hash_ref.c](../NUCLEO-G564RE/oemirot/boot_hal_hash_ref.c#L42)。
