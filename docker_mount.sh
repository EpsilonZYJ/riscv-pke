#!/bin/bash

CONTAINER_NAME=pke_mirror
IMAGE=tjr9098/arm64_pke_mirrors:1.0
HOST_DIR=$(pwd)

# 判断 container 是否存在
if [ "$(docker ps -aq -f name=^${CONTAINER_NAME}$)" ]; then

  # 如果存在但没运行
  if [ "$(docker ps -q -f name=^${CONTAINER_NAME}$)" ]; then
    echo "Container is running, entering..."
  else
    echo "Container exists but stopped, starting..."
    docker start $CONTAINER_NAME
  fi

  docker exec -it $CONTAINER_NAME /bin/bash

else
  echo "Container does not exist, creating..."

  docker run -it \
    --name $CONTAINER_NAME \
    -v "$HOST_DIR":/app \
    -w /app \
    $IMAGE \
    /bin/bash
fi
