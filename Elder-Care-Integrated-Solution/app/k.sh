#!/bin/bash
pid=$(pgrep ecsysapp)
sudo kill -9 $pid
