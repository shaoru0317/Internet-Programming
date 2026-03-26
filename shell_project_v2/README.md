# Simple Shell

一個用 C 語言實作的簡易 Unix Shell，支援基本指令執行、管線（pipe）及 **Numbered Pipe** 功能。

## 功能特色

- **指令執行** — 透過 `fork` + `execvp` 執行外部指令
- **一般管線 (`|`)** — 將前一個指令的 stdout 導向下一個指令的 stdin
- **Numbered Pipe (`|N`)** — 將 stdout 導向後第 N 行指令的 stdin
- **Numbered Pipe with stderr (`!N`)** — 同時將 stdout 與 stderr 導向後第 N 行指令的 stdin
- **內建指令**
  - `setenv VARIABLE [value]` — 設定環境變數
  - `printenv [VARIABLE]` — 印出環境變數（不指定則印出全部）
  - `exit` / `quit` — 離開 Shell

## 專案結構

```
shell_project/
├── Makefile
├── include/
│   └── shell.h        # 共用標頭檔（資料結構、函式宣告）
├── src/
│   ├── main.c         # 主程式迴圈（讀取輸入、呼叫 run_line）
│   ├── exec.c         # 指令解析與 pipeline 執行
│   ├── npipe.c        # Numbered Pipe 管理（建立、查詢、關閉）
│   └── builtin.c      # 內建指令實作
├── object/            # 編譯產生的 .o 檔
└── bin/               # 可放置自訂外部指令
```

## 編譯與執行

```bash
# 編譯
make

# 執行
./shell
```

進入 Shell 後會顯示 `% ` 提示字元，即可輸入指令。

## 使用範例

```bash
% ls
% ls | cat
% ls |2          # 將 ls 的 stdout 導向後第 2 行指令的 stdin
% cat             # （第 1 行，無輸入）
% cat             # （第 2 行，收到前面 ls 的輸出）
% ls !2          # 將 ls 的 stdout+stderr 導向後第 2 行指令的 stdin
% setenv PATH /usr/bin
% printenv PATH
% exit
```

## 環境需求

- GCC
- Linux / macOS（POSIX 相容系統）

## 清除編譯檔案

```bash
make clean
```
