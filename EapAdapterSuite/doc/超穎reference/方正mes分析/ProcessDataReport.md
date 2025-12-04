"upload_process_data": {
    "name": "ProcessDataReport",
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
      "datetime": "timestamp",
      "process_step": "process",
      "work_order": "wo_no",
      "batch_no": "pitch",
      "product_code": "product_id",
      "panel_length": "proc_data.data_item.data_value.panel_lenth",
      "panel_width": "proc_data.data_item.data_value.panel_width",
      "panel_thickness": "proc_data.data_item.data_value.panel_thickness",
      "recipe_name": "proc_data.data_item.data_value.recipe_name",
      "tol_up": "proc_data.data_item.data_value.tol_up",
      "tol_low": "proc_data.data_item.data_value.tol_low",
      "panel_step_info": "proc_data.data_item.data_value.panel_step_info",
      "back_drill_count": "proc_data.data_item.data_value.back_drill_count",
      "tool_split_program": "proc_data.data_item.data_value.tool_split_program",
      "firstArticleMassProduction": "proc_data.data_item.data_value.firstArticleMassProduction",
      "scan_x": "proc_data.data_item.data_value.scan_x",
      "scan_y": "proc_data.data_item.data_value.scan_y",
      "file_drill_c": "proc_data.data_item.data_value.file_drilling_c",
      "file_drill_s": "proc_data.data_item.data_value.file_drilling_s",
      "file_hole_c": "proc_data.data_item.data_value.file_hole_c",
      "file_hole_s": "proc_data.data_item.data_value.file_hole_s"

    },
    "response_mapping": {
      "success": "success",
      "error.message": "error_message",
      "error.details": "error_details"
    }
  },


  "upload_process_data": {
    "datetime": "auto",
    "product_code": "M0E14T445029A01",
    "process_step": "p100",
    "work_order": "id_aa155_p100",
    "batch_no": "pc001",
    "panel_length": "600.1",
    "panel_width": "577.5",
    "panel_thickness": "5.2",
    "recipe_name": "recipeid001",
    "tol_up": "0.32",
    "tol_low": "-0.32",
    "panel_step_info": "B",
    "back_drill_count": "2",
    "tool_split_program": "TC002",
    "firstArticleMassProduction ": 0,
    "scan_x": "202.2",
    "scan_y": "101.1",
    "file_drill_c": "\\192.168.1.1\\file\\drillc.txt",
    "file_drill_s": "\\192.168.1.1\\file\\drills.txt",
    "file_hole_c": "\\192.168.1.1\\file\\holec.txt",
    "file_hole_s": "\\192.168.1.1\\file\\holes.txt"
  },




timestamp:
时间戳

wo_no:
工单号

product_id

料号

pitch

Lot号

process:

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
panel_step_Info:
面次信息
back_drill_count:
背钻次数
toll_split_program:
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
