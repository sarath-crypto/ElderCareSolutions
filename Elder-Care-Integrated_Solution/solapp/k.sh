#!/bin/bash
pid=$(pgrep solapp)
echo "ecsys123" | sudo -S kill -9 $pid
