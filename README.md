# 🔍 Campus Lost & Found — Smart Matching System

A modern campus-wide system for reporting lost and found items, featuring a **C++ smart matching engine** that automatically identifies potential matches between reports.

---

## 📂 Project Structure

```
Campus-Lost-Found/
├── frontend/
│   ├── index.html          # Main dashboard UI
│   ├── style.css           # Design system & responsive styles
│   └── script.js           # Frontend application logic (Vanilla JS)
│
├── backend/
│   ├── main.cpp            # CLI entry point for the matcher
│   ├── matcher.cpp         # Smart matching algorithm implementation
│   └── matcher.h           # Data structures & Matcher class definition
│
├── data/
│   └── items.json          # Shared data store (lost items, found items, matches)
│
└── README.md               # This file
```

## 🚀 How to Run

### Frontend (runs immediately — no build step)

1. Open a terminal in the project root.
2. Start a simple local server:
   ```bash
   # Using Python (if available)
   cd frontend
   python -m http.server 8080

   # OR using Node.js
   npx -y serve frontend

   # OR simply open frontend/index.html directly in your browser
   ```
3. Open `http://localhost:8080` in your browser.

> **Note:** The frontend works fully standalone — it includes embedded sample data as a fallback, so even opening the HTML file directly will display a working UI.

### Backend (C++ Matching Engine)

The C++ backend is the smart matching engine. Compile and run it on Windows:

```bash
# Using g++ (MinGW)
cd backend
g++ -std=c++17 -o campus_matcher.exe main.cpp matcher.cpp
.\campus_matcher.exe ../data/items.json

# Using MSVC (Visual Studio Developer Command Prompt)
cd backend
cl /EHsc /std:c++17 main.cpp matcher.cpp /Fe:campus_matcher.exe
.\campus_matcher.exe ..\data\items.json
```

---

## 🔐 Admin Token Setup

Mutating API endpoints (`/api/add-item`, `/api/delete-item`, `/api/update-match`, `/api/run-matcher`) require an admin token. The server reads it from the `CAMPUS_LF_ADMIN_TOKEN` environment variable — **never store it in source or commit it to git**. If the env var is unset, mutating routes fail-closed with HTTP 503.

### 1. Generate a token

Use any high-entropy random string. Examples:

```powershell
# PowerShell (built-in)
[guid]::NewGuid().ToString() + "-" + [guid]::NewGuid().ToString()
```

Pick something long and unpredictable. Do not reuse a password.

### 2. Set it in PowerShell before starting the server

```powershell
# In the same terminal where you will run server.py
$env:CAMPUS_LF_ADMIN_TOKEN = "your-token-here"
python server.py 8080
```

To verify it took effect, open the dashboard and try to add an item. A correctly-set token returns 201; a missing or wrong token returns 401 (or 503 if the env var was unset).

> **Tip:** To clear the env var in the same session: `Remove-Item Env:\CAMPUS_LF_ADMIN_TOKEN`

### 3. Configure the frontend to send the token

The frontend reads the token from `localStorage` and sends it on every mutating `/api/*` request as the `X-Admin-Token` header. Set it via the browser devtools console before performing any mutating action:

```javascript
localStorage.setItem('campusLF_admin_token', 'your-token-here');
```

Reload the page once after setting it. The token is then attached automatically to `/api/add-item`, `/api/delete-item`, `/api/update-match`, and `/api/run-matcher` requests.

> **Note:** The token lives only in your browser's localStorage. Clear it with `localStorage.removeItem('campusLF_admin_token')` if you need to.

---

## ⚙️ How Smart Matching Works

The C++ engine uses a **weighted multi-criteria scoring algorithm** that compares every lost item against every found item:

| Criterion           | Weight | Method                                  |
|---------------------|--------|-----------------------------------------|
| Category            | 25%    | Exact match                             |
| Location Proximity  | 20%    | Same room → same building → word overlap|
| Description Keywords| 15%    | Tokenized keyword overlap ratio         |
| Color               | 10%    | Exact match                             |
| Brand               | 10%    | Exact match                             |
| Date Proximity      | 10%    | Day difference decay (0→7 days)         |
| Tag Overlap         | 10%    | Jaccard similarity coefficient          |

- Pairs scoring **≥ 30%** are surfaced as potential matches.
- Results are sorted by score (highest first).
- Each match records which criteria contributed to the score.

---

## 🛠 Technology Stack

| Layer      | Technology                    |
|------------|-------------------------------|
| Frontend   | HTML5, CSS3, Vanilla JavaScript |
| Backend    | C++ (C++17 standard)          |
| Data Store | JSON flat file                |
| Icons      | Lucide Icons (CDN)            |
| Fonts      | Inter (Google Fonts)          |

---

## ✨ Features

### Day 1 (Current)
- [x] Modern dark-theme dashboard with responsive design
- [x] Sidebar navigation with section routing
- [x] Stats overview cards with trends
- [x] Lost & Found item listings with category filters
- [x] Smart Match cards with score visualization
- [x] Report Item modal with full form validation
- [x] Item detail view
- [x] Global search across all items
- [x] Activity feed
- [x] Toast notification system
- [x] Mobile-responsive layout
- [x] C++ matching engine structure with full algorithm

### Day 2 (Planned)
- [ ] JSON parser in C++ (load/save items.json)
- [ ] Wire frontend → C++ via JSON file exchange
- [ ] Match confirmation workflow
- [ ] Email notification simulation
- [ ] Item image placeholders

### Day 3 (Planned)
- [ ] Polish & testing
- [ ] Documentation & presentation slides
- [ ] Performance optimizations
- [ ] Edge case handling

---

## 👨‍💻 Team

| Name           | Role               |
|----------------|---------------------|
| Abhishek Y S   | Full-Stack Developer |

---

## 📄 License

This project is created for educational purposes as part of a college assignment.
