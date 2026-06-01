#!/bin/bash
python genwav.py
ffmpeg -i mon.wav -ac 2 on.wav
ffmpeg -i moff.wav -ac 2 off.wav
rm mon.wav -rf
rm moff.wav -rf
