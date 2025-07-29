const BOT_TOKEN = '6999081285:AAGD-lWhDOVj2BZt5170tVzyOqDcCVnYZuk';
const CHAT_ID = '5356315520';
const SHEET_ID = '1uZXlxriaIqNxu0pwdRiTZsKAIEx2xqsXb6nbgypdcag';

// ============ HANDLE POST (TELEGRAM COMMAND) ============
function doPost(e) {
  const contents = JSON.parse(e.postData.contents);
  const messageText = contents.message.text;
  const chatId = contents.message.chat.id;

  if (messageText === '/report' || messageText.toLowerCase() === 'report') {
    const today = Utilities.formatDate(new Date(), Session.getScriptTimeZone(), "yyyy-MM-dd");
    const report = generateRequestReport(today);
    sendTelegramMessage(chatId, report);
  }
}

// ============ HANDLE GET (ESP32/WEMOS DATA INPUT) ============
function doGet(e) {
  const owner = e.parameter.owner;
  const uid = e.parameter.uid;
  const sheet = SpreadsheetApp.openById(SHEET_ID).getActiveSheet();

  const today = Utilities.formatDate(new Date(), Session.getScriptTimeZone(), "yyyy-MM-dd");
  const time = Utilities.formatDate(new Date(), Session.getScriptTimeZone(), "HH:mm:ss");

  if (owner === 'Admin' && uid === 'report_request') {
    const report = generateRequestReport(today);
    sendTelegramMessage(CHAT_ID, report);
    return ContentService.createTextOutput("Report sent to Telegram");
  }

  // Cek apakah perlu menambahkan pemisah harian
  const lastRow = sheet.getLastRow();
  const lastDateCell = sheet.getRange(lastRow, 1).getValue();
  const lastDate = Utilities.formatDate(new Date(lastDateCell), Session.getScriptTimeZone(), "yyyy-MM-dd");

  if (lastDate !== today && lastDate !== "") {
    sheet.appendRow(["-----", "", "", ""]);
    sheet.appendRow([today, "", "", ""]);
  }

  sheet.appendRow([today, time, owner, uid]);
  return ContentService.createTextOutput("Data saved");
}

// ============ GENERATE REPORT ============
function generateRequestReport(dateString) {
  const sheet = SpreadsheetApp.openById(SHEET_ID).getActiveSheet();
  const data = sheet.getDataRange().getValues();
  const display = sheet.getDataRange().getDisplayValues();

  let report = `📅 Laporan Harian\nTanggal: ${dateString}\n\nJam | Nama\n`;
  let found = false;

  for (let i = 1; i < data.length; i++) {
    const rowDate = Utilities.formatDate(new Date(data[i][0]), "GMT+7", "yyyy-MM-dd");
    if (rowDate === dateString && display[i][0] !== "-----") {
      const jam = display[i][1];
      const nama = display[i][2] || "-";
      report += `${jam} | ${nama}\n`;
      found = true;
    }
  }

  if (!found) {
    report += "Data kosong.";
  }

  return report;
}

// ============ SEND MESSAGE TO TELEGRAM ============
function sendTelegramMessage(chatId, text) {
  const url = `https://api.telegram.org/bot${BOT_TOKEN}/sendMessage`;
  const payload = {
    chat_id: chatId,
    text: text,
    parse_mode: "Markdown"
  };

  const options = {
    method: "post",
    contentType: "application/json",
    payload: JSON.stringify(payload)
  };

  UrlFetchApp.fetch(url, options);
}

// ============ AUTOMATIC REPORT SCHEDULER (SET TIME-BASED TRIGGER) ============
function sendDailyReportAtMidnight() {
  const today = Utilities.formatDate(new Date(), Session.getScriptTimeZone(), "yyyy-MM-dd");
  const report = generateRequestReport(today);
  sendTelegramMessage(CHAT_ID, report);

  // Tambahkan separator dan tanggal ke sheet sebagai penanda
  const sheet = SpreadsheetApp.openById(SHEET_ID).getActiveSheet();
  sheet.appendRow(["-----", "", "", ""]);
  sheet.appendRow([today, "", "", ""]);
}
