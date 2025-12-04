"initial_data": {
   "name": "InitialDataReport",
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
     "datetime": "date_time",
     "online_mode": "operation_mode",
     "eqp_status_code": "equipment_status",
     "eqp_status_text": "equipment_status_text",
     "recipe_name": "recipe_name"
   },

   "response_mapping": {
     "success": "is_success",
     "error.message": "error_msg",
     "error.details": "error_details"
   }
 },




 "initial_data": {
   "datetime": "auto",
   "online_mode": "0",
   "eqp_status_code": "2",
   "eqp_status_text": "设备运行中",
   "recipe_name": "recipeid001"
 },


date_time: 时间
operation_mode:操作模式
0 手动，1 自动， 2 半自动

total_wip_count: 设备内产品数量
equipment_status:配方名称


success:
error.message
error.details
