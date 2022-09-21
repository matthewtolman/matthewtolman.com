#!/usr/bin/env bash

find data -name '*.dot' | while IFS=$'\n' read -r FILE; do
  dot -Tsvg $FILE > $(echo $FILE | sed s/\.dot/\.svg/)
done