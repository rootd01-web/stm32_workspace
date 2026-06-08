openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg
    --> kết nối với stm32f1x qua cmsis-dap

openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c "program C:\Project\vinmotion_workspace\stm32_workspace\build\f411-sd-card-debug\sd_card_F411CEU6.elf verify reset exit"
    --> nạp vào mạch stm32f1x qua cmsis-dap

openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program C:/Project/vinmotion_workspace/stm32_workspace/build/f411-sd-card-debug/sd_card_F411CEU6.elf verify reset exit"
    --> nạp vào mạch stm32f1x qua stlink

openocd -f interface/cmsis-dap.cfg -c "transport select swd" -f target/stm32f1x.cfg -c "adapter speed 4000" -c ./path/ten_file_cua_ban.hex verify reset exit"
    --> thêm lệnh "transport select swd" để log nhìn sạch sẽ hơn và tăng tốc độ nạp (adapter speed) để tiết kiệm thời gian

-----------------------------------------------------------

cmake --preset f411-led-blink-debug-win
cmake --build build/f411-led-blink-debug --target flash

-----------------------------------------------------------

arm-none-eabi-gdb ./path/your_project.elf
target extended-remote :3333
monitor reset halt
load
break main
continue

-----------------------------------------------------------

continue	        c
step	            s
next	            n
print <var>	        p <var>
info registers	    i r
backtrace	        bt
where
frame
list                l
break               b
info breakpoints    i b
delete              d

-----------------------------------------------------------

+-------------------------------------------------------+
| 1. App Layer (apps/sd_card_basic/)                    | <- KHÔNG ĐỔI
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
| 2. Service Layer (services/fatfs/)                    | <- KHÔNG ĐỔI
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
| 3. Component Layer (components/sd_card/)              | <- KHÔNG ĐỔI
|    (Chỉ dùng C thuần, không chứa mã HAL cụ thể)       |
+-------------------------------------------------------+
                           |
            [Dùng Interface - Pointer Struct]
                           |
                           v
+-------------------------------------------------------+
| 4. Hardware Porting Layer (drivers/mcu/)              | <- NƠI THAY ĐỔI
|    - Cung cấp các hàm điều khiển GPIO/SPI cụ thể      |
|    - Map phần cứng của MCU hiện tại vào Interface     |
+-------------------------------------------------------+

-----------------------------------------------------------