#!/bin/bash
#
# 红外热成像相机启动脚本
# 用途：启动红外相机程序，需要 root 权限访问 USB 设备
#

echo "===================================================="
echo "  红外热成像相机程序"
echo "  Thermal Camera Application"
echo "===================================================="
echo ""
echo "正在启动相机程序..."
echo "提示：需要 sudo 权限访问 USB 相机设备"
echo ""

# 进入脚本所在目录
cd "$(dirname "$0")"

# 设置库路径
export LD_LIBRARY_PATH=$(pwd)/libs:$LD_LIBRARY_PATH

# 运行程序（需要 sudo 权限）
sudo -E python3 thermal_camera.py

echo ""
echo "程序已退出"
echo "===================================================="
