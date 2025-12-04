"alarm_warning": {
   "name": "AlarmWarningDataReport",
   "method": "POST",
   "direction": "push",
   "enabled": true,
   "enableHeader": true,
   "enableBody": true,

   "header_mapping": {
     "from": "EQP",
     "to": "EAP"
   },
   "body_mapping": {
     "alarm_level": "alarm_type",
     "alarm_status": "report_type",
     "alarm_code": "aw_code",
     "alarm_text": "aw_text"
   },
   "response_mapping": {
     "success": "success",
     "error.message": "error_message",
     "error.details": "error_details"
   }
 },


 "alarm_warning": {
  "alarm_level": "S",
  "alarm_status": "R",
  "alarm_code": "Alarm_HoleIDFail",
  "alarm_text": "定位孔识别失败"
},



alarm_type:
警报形态:
K:客户关注报警
S:报警，立即停止
M：警示
L：警报
N：忽略报警


report_type:
报警形态：
S： 发生
R： 解除


aw_code:

警报代码:
1.Alarm_FeedingTimeout
2.Alarm_CylinderTimeout
3. Alarm_HoleIDFailed
4. Alarm_HoleMatchFailed
5. Alarm_LaserTimeout
6.Alarm_InvalidData
7. Alarm_SafetypDoorOpen
8. Alarm_SafetyGratingTriggerd


aw_text:
警报描述:

1. 上料输送超时
2. 气缸到位超时
3. 定位孔识别失败
4. 定位孔匹配失败
5. 激光获取点云超时
6. 出在无效点云数据
7. 安全门开启
8. 安全光栅被触发
