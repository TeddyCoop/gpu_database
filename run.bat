@echo off
pushd build
call gdb.exe --query="CREATE DATABASE benchmark; IMPORT INTO ten_million_rows_2col FROM 'D:\dev\gpu_database\testing\datasets\ten_million_rows_2col\data.csv'"
popd