#!/bin/bash
pid=$(pgrep ecsysapp)
echo "ecsys123" | sudo -S kill -9 $pid
