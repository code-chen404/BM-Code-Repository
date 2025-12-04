"heartbeat": {
  "name": "AreYouThereRequest",
  "method": "POST",
  "direction": "push",
  "enabled": true,
  "enableHeader": true,
  "enableBody": true,
  "timeoutMs": 5000,
  "retryCount": 3,

  "header_mapping": {
    "from": "EQP",
    "to": "EAP"
  },

  "body_mapping": {
    "device_id": "user_id",
    "device_ip": "server_ip"
  },

  "response_mapping": {
    "success": "is_success",
    "error.message": "error_msg",
    "error.details": "error_details",
    "result.user_id": "device_id",
    "result.server_ip": "device_ip"
  }
}



"heartbeat": {
   "device_id": "ID001",
   "device_ip": "192.168.1.100"
 }



 header:
 messsagename: 接口名字
 timestamp:时间.毫秒
 from: EQP\EAP
 to： EAP\EQP
 token: 密钥

 body:

 user_id: 设备ID
 server_ip: 设备IP
 
