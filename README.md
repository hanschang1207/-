# -

## 小滿食堂候位登記

以 C++（Winsock）提供本機 HTTP 服務，並使用網頁介面管理餐廳候位名單。

## 功能

- 登記候位者姓名與電話，並取得候位號碼
- 查看候位名單、查詢順位、預覽下一位及叫號
- 在候位隊伍區依序顯示等待者與電話
- 完成候位或於打烊時清空名單
- 候位資料保存在本機 `waitlist.tsv`

## 執行需求

- Windows
- `g++`（支援 C++17，並可連結 Winsock）

## 啟動

在檔案總管中執行 `start-waitlist.bat`。批次檔會編譯 C++ 服務、啟動服務並開啟瀏覽器。

也可以在 PowerShell 中手動執行：

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic server.cpp -o waitlist-server.exe -lws2_32
./waitlist-server.exe
```

服務啟動後，開啟 <http://127.0.0.1:8080/>。按 `Ctrl+C` 可停止服務。

## 資料與隱私

`waitlist.tsv` 會包含候位者姓名和電話，只保存在執行服務的資料夾，不應提交到 GitHub。此檔案已加入 `.gitignore`。執行檔由啟動批次檔編譯產生，也不需提交。

服務僅監聽本機 `127.0.0.1`，不提供外部網路連線。