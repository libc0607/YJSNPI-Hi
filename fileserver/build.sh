#!/bin/sh
CGO_ENABLED=0 GOOS=linux GOARCH=arm go build -ldflags="-s -w" fileserver.go
#upx -9 fileserver
