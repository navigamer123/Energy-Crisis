#!/usr/bin/env python3
"""
Arcade Token & Credit Emulator for Energy Crisis / ArcadeTemplate.x86_64
Matches the exact SQLite specification from CreditsManager.cs:
  - Table: settings (key TEXT PRIMARY KEY, value TEXT)
  - Key: 'credits'
  - DB Path: $DHO_DB_PATH or fallback assets/test_db.db
"""

import sys
import os
import sqlite3
import time

def resolve_db_path():
    env_path = os.environ.get("DHO_DB_PATH")
    if env_path and os.path.exists(env_path):
        return env_path
    
    # Check assets/test_db.db relative to current dir or script dir
    candidates = [
        "assets/test_db.db",
        os.path.join(os.path.dirname(__file__), "..", "assets", "test_db.db"),
        os.path.abspath("assets/test_db.db")
    ]
    for c in candidates:
        if os.path.exists(c):
            return os.path.abspath(c)
    
    # Default to assets/test_db.db in current working directory
    target = os.path.abspath("assets/test_db.db")
    os.makedirs(os.path.dirname(target), exist_ok=True)
    return target

def init_db(db_path):
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    cur.execute("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT);")
    cur.execute("INSERT OR IGNORE INTO settings (key, value) VALUES ('credits', '0');")
    conn.commit()
    conn.close()

def get_credits(db_path):
    init_db(db_path)
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    cur.execute("SELECT value FROM settings WHERE key = 'credits' LIMIT 1;")
    row = cur.fetchone()
    conn.close()
    if not row or not row[0]:
        return 0
    val_str = str(row[0]).strip().strip("'\"")
    try:
        return int(val_str)
    except ValueError:
        return 0

def set_credits(db_path, amount):
    init_db(db_path)
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    cur.execute("""
        INSERT INTO settings(key, value) VALUES('credits', ?)
        ON CONFLICT(key) DO UPDATE SET value = excluded.value;
    """, (str(amount),))
    conn.commit()
    conn.close()
    return amount

def add_credits(db_path, delta):
    current = get_credits(db_path)
    new_val = max(0, current + delta)
    set_credits(db_path, new_val)
    return new_val

def run_tests(db_path):
    print("=" * 60)
    print("  RUNNING CREDITS / TOKEN EMULATOR AUTOMATED TESTS")
    print("=" * 60)
    print(f"[Test] Using Database: {db_path}")

    # 1. Initialize DB
    init_db(db_path)
    print("[Test 1/5] Database schema & settings table verification: OK")

    # 2. Set credits
    set_credits(db_path, 5)
    c = get_credits(db_path)
    assert c == 5, f"Expected 5 credits, got {c}"
    print(f"[Test 2/5] Direct set to 5 credits: OK (Got {c})")

    # 3. Add coin (+1)
    c = add_credits(db_path, 1)
    assert c == 6, f"Expected 6 credits, got {c}"
    print(f"[Test 3/5] Add coin (+1): OK (New total: {c})")

    # 4. Consume coin (-1)
    c = add_credits(db_path, -1)
    assert c == 5, f"Expected 5 credits, got {c}"
    print(f"[Test 4/5] Consume coin (-1): OK (New total: {c})")

    # 5. Zero out and test depletion
    set_credits(db_path, 0)
    c = get_credits(db_path)
    assert c == 0, f"Expected 0 credits, got {c}"
    print(f"[Test 5/5] Zero credits depletion check: OK (Got {c})")

    # Reset to default 2 credits for playing
    set_credits(db_path, 2)
    print(f"[Test] Reset database to default 2 credits.")
    print("=" * 60)
    print("  ALL 5 TOKEN EMULATION TESTS PASSED!")
    print("=" * 60)

def interactive_mode(db_path):
    print("=" * 60)
    print("  ARCADE COIN / TOKEN ACCEPTOR EMULATOR")
    print(f"  Database: {db_path}")
    print(f"  Current credits: {get_credits(db_path)}")
    print("=" * 60)
    print("  Commands:")
    print("    [Enter]    -> Insert Coin (+1 credit)")
    print("    [+]        -> Insert Coin (+1 credit)")
    print("    [-]        -> Consume Coin (-1 credit)")
    print("    [0]        -> Reset to 0 credits")
    print("    [s]        -> Show status")
    print("    [q]        -> Quit")
    print("=" * 60)
    
    try:
        while True:
            cmd = input(f"[Credits: {get_credits(db_path)}] Enter command: ").strip().lower()
            if cmd == "q" or cmd == "exit":
                print("Exiting emulator.")
                break
            elif cmd == "" or cmd == "+":
                new_c = add_credits(db_path, 1)
                print(f"--> [COIN INSERTED] Credit added (+1). Total credits: {new_c}")
            elif cmd == "-":
                new_c = add_credits(db_path, -1)
                print(f"--> [COIN CONSUMED] Credit deducted (-1). Total credits: {new_c}")
            elif cmd == "0":
                new_c = set_credits(db_path, 0)
                print(f"--> [RESET] Credits set to: 0")
            elif cmd == "s" or cmd == "status":
                print(f"--> Database: {db_path} | Credits: {get_credits(db_path)}")
            else:
                try:
                    num = int(cmd)
                    new_c = set_credits(db_path, num)
                    print(f"--> Credits set to: {new_c}")
                except ValueError:
                    print("Unknown command. Press [Enter] to insert coin or 'q' to quit.")
    except (KeyboardInterrupt, EOFError):
        print("\nExiting emulator.")

def main():
    db_path = resolve_db_path()

    if len(sys.argv) <= 1:
        interactive_mode(db_path)
        return

    arg = sys.argv[1].lower()
    if arg in ("-h", "--help", "help"):
        print("Usage:")
        print("  python3 scripts/arcade_token_emulator.py               (Interactive mode)")
        print("  python3 scripts/arcade_token_emulator.py status        (Show current credits)")
        print("  python3 scripts/arcade_token_emulator.py add [N]       (Add N credits, default 1)")
        print("  python3 scripts/arcade_token_emulator.py consume [N]   (Consume N credits, default 1)")
        print("  python3 scripts/arcade_token_emulator.py set <N>       (Set credits to N)")
        print("  python3 scripts/arcade_token_emulator.py test          (Run test suite)")
        return

    if arg in ("status", "info"):
        print(f"Database Path: {db_path}")
        print(f"Current Credits: {get_credits(db_path)}")
    elif arg in ("add", "insert", "+"):
        amount = int(sys.argv[2]) if len(sys.argv) > 2 else 1
        new_val = add_credits(db_path, amount)
        print(f"[CreditsManager] Added {amount} credit(s). Total: {new_val}")
    elif arg in ("consume", "spend", "-"):
        amount = int(sys.argv[2]) if len(sys.argv) > 2 else 1
        new_val = add_credits(db_path, -amount)
        print(f"[CreditsManager] Consumed {amount} credit(s). Total: {new_val}")
    elif arg in ("set", "="):
        if len(sys.argv) < 3:
            print("Error: specify amount to set, e.g.: set 5")
            sys.exit(1)
        amount = int(sys.argv[2])
        set_credits(db_path, amount)
        print(f"[CreditsManager] Set credits to: {amount}")
    elif arg in ("test", "tests", "run-tests"):
        run_tests(db_path)
    elif arg in ("interactive", "i"):
        interactive_mode(db_path)
    else:
        print(f"Unknown command: {arg}. Run with --help for usage.")

if __name__ == "__main__":
    main()
