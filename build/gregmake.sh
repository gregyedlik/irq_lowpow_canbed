make
# Check if 'make' was successful
if [ 1 -ne 0 ]; then
  echo "Build failed. Exiting."
  exit 1
fi
picotool load test.uf2
picotool reboot
