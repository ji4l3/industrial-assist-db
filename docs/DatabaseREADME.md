# Industrial Assist 数据库说明

本项目使用 **SQLite** 作为数据库，结合 Qt 的 `QSqlDatabase` 和 `QSqlQuery` 进行访问。  
数据库主要存储 **用户、设备、工单、工单日志、设备时序数据、视频会议、参会人员与聊天记录** 等信息。

---

## 📍 数据库文件位置
- **Linux (Ubuntu)**: `~/.local/share/remotework/industrial_assist.db`  
- **Windows**: `%AppData%\remotework/industrial_assist.db`  
- **macOS**: `~/Library/Application Support/remotework/industrial_assist.db`

⚠️ **重置数据库**：删除上述路径的 `.db` 文件（以及 `.db-wal` / `.db-shm`），下次运行程序会自动重建。

---

## 🗂️ 表结构概览

### 1. `users` —— 用户
| 字段 | 类型 | 说明 |
|------|------|------|
| user_id | INTEGER PK | 用户 ID |
| username | TEXT UNIQUE | 登录名 |
| password_hash | TEXT | 密码哈希 |
| full_name | TEXT | 姓名 |
| role | TEXT | expert/requester/admin |
| phone / email | TEXT | 联系方式 |
| created_at | INTEGER | 创建时间 (epoch 秒) |

---

### 2. `devices` —— 设备
| 字段 | 类型 | 说明 |
|------|------|------|
| device_id | INTEGER PK | 设备 ID |
| sn | TEXT UNIQUE | 序列号 |
| model / location / owner_org | TEXT | 型号、位置、单位 |
| created_at | INTEGER | 登记时间 |

---

### 3. `work_orders` —— 工单
| 字段 | 类型 | 说明 |
|------|------|------|
| order_id | INTEGER PK | 工单 ID |
| requester_id | FK → users.user_id | 申请人 |
| expert_id | FK → users.user_id | 专家 (可空) |
| device_id | FK → devices.device_id | 设备 |
| status | TEXT | open / assigned / in_progress / paused / resolved / closed / canceled |
| priority | INTEGER | 1–5 |
| title / description | TEXT | 标题、描述 |
| created_at / assigned_at / closed_at | INTEGER | 时间戳 |

---

### 4. `order_logs` —— 工单日志
| 字段 | 类型 | 说明 |
|------|------|------|
| log_id | INTEGER PK | 日志 ID |
| order_id | FK → work_orders.order_id | 所属工单 |
| actor_id | FK → users.user_id | 执行人 (可空) |
| action / content | TEXT | 动作、内容 |
| created_at | INTEGER | 时间戳 |

---

### 5. `device_data` —— 设备时序数据
| 字段 | 类型 | 说明 |
|------|------|------|
| data_id | INTEGER PK | 数据 ID |
| device_id | FK → devices.device_id | 设备 |
| ts | INTEGER | 时间戳 (epoch 秒) |
| metric | TEXT | 指标名 (如 temperature, pressure) |
| value_num | REAL | 数值型 |
| value_text | TEXT | 文本型 |
| 约束 | UNIQUE(device_id, ts, metric) |

---

### 6. `video_meetings` —— 视频会议
| 字段 | 类型 | 说明 |
|------|------|------|
| meeting_id | INTEGER PK | 会议 ID |
| order_id | FK → work_orders.order_id | 所属工单 |
| start_time / end_time | INTEGER | 开始/结束时间 |
| topic | TEXT | 主题 |
| created_at | INTEGER | 创建时间 |

---

### 7. `meeting_participants` —— 会议参与者
| 字段 | 类型 | 说明 |
|------|------|------|
| participant_id | INTEGER PK | 记录 ID |
| meeting_id | FK → video_meetings.meeting_id | 所属会议 |
| user_id | FK → users.user_id | 用户 |
| role | TEXT | 角色 (主持人/专家/申请人) |
| joined_at | INTEGER | 加入时间 |
| 约束 | UNIQUE(meeting_id, user_id) |

---

### 8. `chat_messages` —— 聊天记录（v4 新增）
| 字段 | 类型 | 说明 |
|------|------|------|
| message_id | INTEGER PK | 消息 ID |
| order_id | FK → work_orders.order_id | 工单 ID |
| sender_id | FK → users.user_id | 发送人 |
| receiver_id | FK → users.user_id | 接收人 (可空，群聊可空) |
| content | TEXT | 消息文本 |
| content_type | TEXT | 类型 (text/image/file/system) |
| attachment | TEXT | 附件路径/URL |
| is_read | INTEGER | 是否已读 (0/1) |
| created_at | INTEGER | 发送时间 |
| read_at / edited_at | INTEGER | 已读/编辑时间 |
| deleted | INTEGER | 是否删除 (软删) |

---

## 🔗 ER 图
见 `docs/ER_diagram.png` （已包含聊天消息表）。

---

## 💬 聊天触发器 (v5 新增)
为保证消息发送人必须是工单的申请人或专家，数据库定义了触发器：

```sql
CREATE TRIGGER IF NOT EXISTS trg_chat_sender_valid
BEFORE INSERT ON chat_messages
BEGIN
  SELECT CASE WHEN NOT EXISTS (
    SELECT 1 FROM work_orders
    WHERE order_id = NEW.order_id
      AND (requester_id = NEW.sender_id OR expert_id = NEW.sender_id)
  )
  THEN RAISE(ABORT, 'Sender not a participant of this work order') END;
END;

