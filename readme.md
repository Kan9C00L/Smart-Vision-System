# 🚀 基于边缘计算与 YOLOv8 的多模态无线视觉感知系统

## 📖 项目简介

本项目是一个面向智能安防与物联网场景的端到端（End-to-End）视觉感知系统。系统采用“边缘采集 + 中心推理 + 终端反馈”的分布式架构：前端利用低功耗微控制器（ESP32-CAM）作为无线视觉节点，通过 Wi-Fi 传输实时视频流；中心端基于 PC 端的 GPU（RTX 4060）运行 YOLOv8 深度学习模型进行高帧率推理；决策结果通过 TCP Socket 协议下发至末端执行节点（ESP32），实现物理层的声光报警与多模态环境数据的闭环联动。

**💡 潜在应用场景**：低成本智能看家、工业危险区域防闯入、**智能门锁/门禁防尾随系统**。

## 📸 演示效果

*(请确保根目录下存在 demo.gif)*
![系统演示](demo.gif)

## 🏗️ 系统架构与核心算法工作流

整个系统采用“云-边-端”协同架构，通过多模态数据融合实现复杂的业务决策。核心工作流如下：

```mermaid
graph TD
    classDef perception fill:#e1f5fe,stroke:#0288d1,stroke-width:2px;
    classDef network fill:#fff3e0,stroke:#f57c00,stroke-width:2px;
    classDef ai fill:#f3e5f5,stroke:#8e24aa,stroke-width:2px;
    classDef logic fill:#ffebee,stroke:#d32f2f,stroke-width:2px;
    classDef actuator fill:#e8f5e9,stroke:#388e3c,stroke-width:2px;

    subgraph 感知层 [🟢 边缘感知层 Edge Perception]
        CAM[ESP32-CAM 视觉采集]:::perception
        SENSORS[光敏/热敏 NTC 物理传感器]:::perception
    end

    subgraph 网络层 [🔵 网络通信层 Network & Comm]
        HTTP[HTTP MJPEG 视频流推流]:::network
        TCP[TCP Socket 双向异步通信]:::network
    end

    subgraph 推理层 [🟣 核心推理层 AI Inference]
        GPU[PC端 RTX 4060 GPU 算力]:::ai
        YOLO[YOLOv8 目标检测模型]:::ai
        NMS[NMS 非极大值抑制 + 置信度过滤]:::ai
    end

    subgraph 决策层 [🔴 业务决策层 Logic & Decision]
        DEBOUNCE[积分防抖算法 / 状态机]:::logic
        FUSION[多模态融合判定\n(视觉+环境数据)]:::logic
    end

    subgraph 执行层 [🟡 终端执行层 Actuator]
        ESP32[ESP32 主控节点]:::actuator
        ALARM[蜂鸣器声光报警]:::actuator
        OLED[OLED 状态显示]:::actuator
    end

    CAM -->|实时无线画面| HTTP
    HTTP -->|OpenCV 视频流拉取| GPU
    GPU --> YOLO --> NMS
    NMS -->|输出目标坐标与类别| DEBOUNCE
    
    SENSORS -->|ADC 数据采集| TCP
    TCP -->|环境温湿度/光照同步| FUSION
    
    DEBOUNCE -->|防误报输出| FUSION
    FUSION -->|生成控制指令| TCP
    TCP -->|下发 ALERT_ON/OFF| ESP32
    ESP32 --> ALARM
    ESP32 --> OLED
   ```

## 🧠 核心算法模块解析
1. 图像采集层（ESP32-CAM）
硬件：ESP32-CAM（OV2640 传感器）。

底层优化：针对 OV2640 传感器在长排线传输中易产生的高频电磁干扰（横条纹），在固件层修改 XCLK 时钟频率（降至 10MHz）并强制关闭镜头校正（Lenc），从物理底层压制噪声，保证图像数据的可用性。

推流协议：基于 HTTP MJPEG 协议，将视频流封装为局域网可访问的 URL（:81/stream），实现低延迟的无线视频流分发。

2. 推理与后处理层（PC端）
算法框架：采用 PyTorch 框架与 Ultralytics YOLOv8 核心库。

硬件加速：调用 CUDA 11.8 与 RTX 4060 GPU，对 YOLOv8 模型进行硬件加速，保障实时推理（约 15-20 FPS）。

后处理算法 (NMS)：由于低画质摄像头易产生重叠误检，引入了非极大值抑制（NMS）算法与动态置信度阈值（conf=0.35），过滤掉低置信度的“幽灵框”，仅保留高概率目标。

3. 鲁棒性控制层（积分防抖机制）
针对 AI 视觉常出现的“单帧漏检”问题，设计了积分防抖机制：

当检测到目标时，系统计数器 +2；未检测到目标时，计数器 -1。

设置阈值触发（如 Score >= 4 才报警），避免了因画面闪烁或摄像头掉帧导致的蜂鸣器频繁开关，大幅提升了系统的工业鲁棒性。

4. 异构双向通信层（TCP Socket）
架构：基于 Python 的 socket 库搭建 TCP Server，ESP32 作为 Client。

双向数据流：

下行：PC 将视觉检测结果（ALERT_ON / ALERT_OFF）封装为字节流，精准下发至 ESP32 控制蜂鸣器与 OLED。

上行：ESP32 结合 NTC 热敏电阻（Beta 公式）与光敏电阻，将环境数据实时回传至 PC，实现视觉与物理传感器的多模态数据融合。

## 🚪 业务场景扩展：智能门锁与门禁系统
本系统架构具有极高的可扩展性，稍加改造即可应用于智能门锁/玄关监控场景：

防尾随与徘徊检测：设定 ROI（感兴趣区域），当门外有人停留超过 5 秒（结合防抖积分判断），触发门内屏报警。

手势/人脸识别融合：后续可接入 MediaPipe，当摄像头识别到特定的手势（如“OK”），或识别到已录入的家人面部时，通过 Wi-Fi 向 ESP32 发送 UNLOCK 指令，控制继电器模拟开锁。

光线自适应门禁：白天正常监控；夜间（结合光敏电阻）自动开启最高级别警戒，一旦视觉检测到人靠近，立即亮起门禁背光灯并语音提示。

## 🛠️ 硬件与软件技术栈
视觉算法：Python 3.9, OpenCV, PyTorch, YOLOv8, Numpy

网络通信：TCP Socket, HTTP 流媒体协议

嵌入式硬件：ESP32, ESP32-CAM (OV2640), 0.96寸 OLED (I2C), 有源蜂鸣器, 光敏/热敏电阻

开发环境：Windows 11, WSL2 (Ubuntu 22.04), Git, CUDA 11.8