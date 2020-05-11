# Hisilicon Hi35xx sample Makefile
# YJSNPI FPV VENC
include ../Makefile.param

SRCS := $(wildcard *.c)
TARGET := $(SRCS:%.c=%)

TARGET_PATH := $(PWD)

# compile linux or HuaweiLite
include $(PWD)/../$(ARM_ARCH)_$(OSTYPE).mak
