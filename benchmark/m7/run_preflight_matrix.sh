#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 RESULTS_DIR" >&2
  exit 2
fi

aimer_results_dir=$1
aimer_repo_dir=$(cd ../.. && pwd)
aimer_openocd=/private/tmp/aimer-m7-tools/openocd/bin/openocd
aimer_stlink_serial=066AFF565380535067105130
aimer_uart_port=/dev/cu.usbmodem11403

mkdir -p "$aimer_results_dir/preflight"

for aimer_impl in reference optimized; do
  for aimer_param in 128f 128s 192f 192s 256f 256s; do
    aimer_name=$aimer_impl-$aimer_param
    aimer_elf=$aimer_repo_dir/benchmark/m7/build/$aimer_name/preflight.elf
    aimer_flash_log=$aimer_results_dir/preflight/$aimer_name-flash.log
    aimer_uart_log=$aimer_results_dir/preflight/$aimer_name-uart.log
    echo "running $aimer_name"
    "$aimer_openocd" \
      -f board/st_nucleo_f7.cfg \
      -c "adapter serial $aimer_stlink_serial" \
      -c init \
      -c "reset halt" \
      -c "program $aimer_elf verify reset exit" \
      >"$aimer_flash_log" 2>&1
    python3 capture_serial.py "$aimer_uart_port" --timeout 900 \
      --output "$aimer_uart_log"
  done
done
