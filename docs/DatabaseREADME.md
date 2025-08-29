# Industrial Assist 数据库说明

本项目使用 **SQLite** 作为数据库，结合 Qt 的 `QSqlDatabase` 和 `QSqlQuery` 进行访问。  
数据库主要存储 **用户、设备、工单、工单日志、设备时序数据、视频会议与参会人员** 等信息。

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

## 🔗 ER 图
见 `docs/ER_diagram.png`  

---

## 💻 常用 SQL 示例

### 插入用户
```sql
INSERT INTO users(username, password_hash, role, full_name)
VALUES('req_li', 'argon2id$xxx', 'requester', '李四');


