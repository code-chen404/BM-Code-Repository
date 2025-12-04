"equipment_information": {
   "name": "EquipmentInformation",
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
     "online_mode": "operation_mode",
     "recipe_name": "recipe_name"
   },
   "response_mapping": {
     "success": "success",
     "error.message": "error_message",
     "error.details": "error_details"
   }
 },


 "equipment_information": {
    "online_mode": "0",
    "recipe_name": "recipeid001"
  },




operation_mode: 0 手动模式
1 自动模式
2. 半自动模式

recipe_name:
配方名称
Line_CLearning:

设备清线完成 [1. 完成 , 2 失败]
