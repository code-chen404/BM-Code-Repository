online_mode:
在线模式
recipe_name：
配方名称

eqp_status_code：
设备状态
eqp_status_text:
设备状态文字


alarm_level:
警报形态:
K:客户关注报警
S:报警，立即停止
M：警示
L：警报
N：忽略报警


alarm_status:
报警形态：
S： 发生
R： 解除


alarm_code:

警报代码:
1.Alarm_FeedingTimeout
2.Alarm_CylinderTimeout
3. Alarm_HoleIDFailed
4. Alarm_HoleMatchFailed
5. Alarm_LaserTimeout
6.Alarm_InvalidData
7. Alarm_SafetypDoorOpen
8. Alarm_SafetyGratingTriggerd


alarm_text:

警报描述:

1. 上料输送超时
2. 气缸到位超时
3. 定位孔识别失败
4. 定位孔匹配失败
5. 激光获取点云超时
6. 出在无效点云数据
7. 安全门开启
8. 安全光栅被触发










device_id:
设备编码

process_step:

工序编码(工序小站)


work_order: 工单号

batch_no ：Lot号

para_list ： 数组

para_name: 参数名
para_value: 参数值

back_drill_count: 背钻次数

tool_split_program: 分刀程式

firstArticleMassProduction: 首件/量产


product_code:
料号

work_order:
工单号

batch_no:
Lot号

recipe:
配方名称

process_step
工序小站

panel_length:
板长
panel_width:
板宽

panel_thickness:

板厚

parameter_list:
参数清单list

parameter_name:
参数名
parameter_value:
参数值

tol_up

上公差

tol_low
下公差

scan_x
Panel 读码坐标x （为空表示设备无需读码）
scan_y
Panel 读码坐标y (为空表示设备无需读码)

panel_step_info:
面次信息(C-正面 ,S 反面)

back_drill_count:
背钻次数

tool_split_program:

分刀程式

file_hole_c:

C面定位孔文件路径/程式名:

file_drill_c:
C面钻带文件路径/程式名

file_hole_s:
S面定位孔文件路径/程式名

file_drill_s:
S面钻带文件路径/程式名


////proccessDataReport////

datetime:
时间戳

work_order:
工单号

product_code

料号

batch_no

Lot号

process_step:

工序代码(工序小站)

proc_data:
参数list-lot级

data_value:
参数值-lot级
data_item:
参数名-lot级

recipe_name:
配方名称

tol_up:
上公差
tol_low:
下公差
panel_thickness:
板厚
panel_length:
板长
panel_width:
板宽
panel_step_info:
面次信息
back_drill_count:
背钻次数
tool_split_program:
分刀程式
file_hole_c:
C面定位孔文件路径
file_drilling_c:
C面钻带文件路径
file_hole_s:
S面定位孔文件路径
file_drilling_s:
S面钻带文件路径
scan_x:
二维码X坐标
scan_y:
二维码y坐标
firstArticleMassProduction:
首件/量产







///////////// Panel Data Report /////

datetime:
时间戳

work_order:
工单号

product_code:
料号

proc_data:
参数list-panel级
data_value:
参数值-panel级
data_item:
参数名-panel级

measure_result:
检测结果
drillfile_savepathname_c:
补偿文件路径-C面
drillfile_savepathname_s:
补偿文件路径-S面
datafile_savepathname_c:
数据文件路径-C面
即待解析csv 需包含文件名，文件名需以panel码命名

datafile_savepathname_s:
数据文件路径-S面
