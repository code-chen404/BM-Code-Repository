"current_time": {
    "name": "EquipmentCurrentDateTime",
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
      "datetime": "date_time"
    },

    "response_mapping": {
      "success": "is_success",
      "error.message": "error_msg",
      "error.details": "error_details"
    }
  },




  "current_time": {
    "datetime": "auto"
  },


  date_time：时间
  
