"equipment_status": {
    "name": "EquipmentStatus",
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
      "eqp_status_code": "equipment_status",
      "eqp_status_text": "equipment_status_text"
    },
    "response_mapping": {
      "success": "success",
      "error.message": "error_message",
      "error.details": "error_details"
    }
  },



  "equipment_status": {
   "eqp_status_code": "2",
   "eqp_status_text": "设备运行中"
 },

 
