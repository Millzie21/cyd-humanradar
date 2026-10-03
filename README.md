# cyd-humanradar
Turns 2.8 inch cyd (cheap yellow display) into a 3 target human radar using a ld2450 mmwave radar module. 

In the ino source code you can set #define SIMULATE to true to show blips if you do not have radar module yet, however if you have it ensure it is set to false otherwise you would get fake data.

Wiring
| CYD | LD2450 | 
| --- | --- | 
| GPIO 27 (RX) | TX |
| GPIO 22 (TX) | RX | 

Parts required.
| LD2450 radar | [LD2450]([https://www.example.com](https://www.aliexpress.com/item/1005007254785237.html?spm=a2g0o.productlist.main.1.1a6c15b2vrlaSY&algo_pvid=cf8f82c4-784a-4bba-a35b-54a93075159c&algo_exp_id=cf8f82c4-784a-4bba-a35b-54a93075159c-0&pdp_ext_f=%7B"order"%3A"4307"%2C"eval"%3A"1"%2C"fromPage"%3A"search"%7D&pdp_npi=6%40dis%21AUD%217.76%211.45%21%21%215.29%210.99%21%402101dedf17910689307098198e0d4d%2112000039961554064%21sea%21AU%210%21ABX%211%210%21n_tag%3A-29910%3Bd%3Add158f5f%3Bm03_new_user%3A-29895%3BpisId%3A5000000210900741&curPageLogUid=eYuWuS8cwoZf&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005007254785237%7C_p_origin_prod%3A))|
| CYD 2.8 inch |[CYD]([https://www.example.com](https://www.aliexpress.com/item/1005007254785237.html?spm=a2g0o.productlist.main.1.1a6c15b2vrlaSY&algo_pvid=cf8f82c4-784a-4bba-a35b-54a93075159c&algo_exp_id=cf8f82c4-784a-4bba-a35b-54a93075159c-0&pdp_ext_f=%7B"order"%3A"4307"%2C"eval"%3A"1"%2C"fromPage"%3A"search"%7D&pdp_npi=6%40dis%21AUD%217.76%211.45%21%21%215.29%210.99%21%402101dedf17910689307098198e0d4d%2112000039961554064%21sea%21AU%210%21ABX%211%210%21n_tag%3A-29910%3Bd%3Add158f5f%3Bm03_new_user%3A-29895%3BpisId%3A5000000210900741&curPageLogUid=eYuWuS8cwoZf&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005007254785237%7C_p_origin_prod%3A)](https://www.aliexpress.com/item/1005007286667162.html?spm=a2g0o.productlist.main.5.6bfe37b0oJI0Cm&algo_pvid=05b7b77e-36df-414a-be8a-ab714b56cb19&algo_exp_id=05b7b77e-36df-414a-be8a-ab714b56cb19-4&pdp_ext_f=%7B"order"%3A"9478"%2C"spu_best_type"%3A"price"%2C"eval"%3A"1"%2C"fromPage"%3A"search"%7D&pdp_npi=6%40dis%21AUD%2114.47%211.49%21%21%219.86%211.02%21%402103138417910692116255803e1026%2112000040072773072%21sea%21AU%210%21ABX%211%210%21n_tag%3A-29910%3Bd%3Add158f5f%3Bm03_new_user%3A-29895%3BpisId%3A5000000218631894&curPageLogUid=NDTAOgh9Ktgg&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005007286667162%7C_p_origin_prod%3A))
