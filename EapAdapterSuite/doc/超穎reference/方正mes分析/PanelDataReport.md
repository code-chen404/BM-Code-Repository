"upload_panel_data": {
   "name": "PanelDataReport",
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
     "product_code": "product_id",
     "process_step": "process",
     "work_order": "wo_no",
     "batch_no": "pitch",
     "panel_id": "panel_list.panel_id",
     "measure_result": "panel_list.proc_data.data_item.data_value.mea_result",
     "drillfile_savepathname_c": "panel_list.proc_data.data_item.data_value.file_pathname_c",
     "drillfile_savepathname_s": "panel_list.proc_data.data_item.data_value.file_pathname_s",
     "datafile_savepathname_c": "panel_list.proc_data.data_item.data_value.datafile_pathname_c",
     "datafile_savepathname_s": "panel_list.proc_data.data_item.data_value.datafile_pathname_s"
   },
   "response_mapping": {
     "success": "success",
     "error.message": "error_message",
     "error.details": "error_details"
   }
 }


 "upload_panel_data": {
    "datetime": "auto",
    "product_code": "M0E14T445029A01",
    "process_step": "p101",
    "work_order": "N01-10093073-0000-000",
    "batch_no": "003",
    "panel_id": "15445148",
    "measure_result": "OK",
    "drillfile_savepathname_c": "\\192.168.1.1\\file\\drillc.txt",
    "drillfile_savepathname_s": "\\192.168.1.1\\file\\drills.txt",
    "datafile_savepathname_c": "\\192.168.1.1\\file\\datac.txt",
    "datafile_savepathname_s": "\\192.168.1.1\\file\\datas.txt"
  }
