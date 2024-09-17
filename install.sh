git clone https://github.com/raspberrypi/pico-sdk.git
git clone https://github.com/raspberrypi/pico-extras.git
git clone https://github.com/gregyedlik/pico-mcp2515.git
cp pico-sdk/external/pico_sdk_import.cmake .
cp pico-extras/external/pico_extras_import.cmake .
mkdir build
cat << EOF > build/gregmake.sh
make
# Check if 'make' was successful
if [ $? -ne 0 ]; then
  echo "Build failed. Exiting."
  exit 1
fi
picotool load test.uf2
picotool reboot
EOF
chmod +x build/gregmake.sh
