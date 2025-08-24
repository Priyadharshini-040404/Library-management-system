#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
using namespace std;

// Global handles
SQLHENV env = NULL;
SQLHDBC dbc = NULL;
string currentUserRole;
void addBook();
void updateBook();
void deleteBook();
void viewMembers();
void searchMembers();



// Utility function to show SQL errors
void showError(const char* fn, SQLHANDLE handle, SQLSMALLINT type) {
    SQLINTEGER i = 0, native;
    SQLCHAR state[7], text[256];
    SQLSMALLINT len;
    while (SQLGetDiagRec(type, handle, ++i, state, &native, text, sizeof(text), &len) == SQL_SUCCESS)
        cerr << fn << " error: " << state << " " << native << " " << text << "\n";
}

// Check if record exists
bool exists(const string& sql) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false;
    }
    SQLRETURN r = SQLFetch(stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO);
}

// Execute SQL command
bool execSQL(const string& sql) {
    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    SQLRETURN ret = SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) showError("execSQL", stmt, SQL_HANDLE_STMT);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return SQL_SUCCEEDED(ret);
}
// ===== Login =====
bool login() {
    string username, password;
    cout << "\n=== Login ===\nUsername: ";
    getline(cin, username);
    cout << "Password: ";
    getline(cin, password);

    string sql = "SELECT role FROM users WHERE username='" + username + 
             "' AND passwordhash='" + password + "'";


    SQLHSTMT stmt;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt))) return false;
    if (!SQL_SUCCEEDED(SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS))) {
        showError("Login", stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt); return false;
    }

    SQLCHAR role[20], status[20];
    if (SQLFetch(stmt) == SQL_SUCCESS) {
        SQLGetData(stmt, 1, SQL_C_CHAR, role, sizeof(role), NULL);
        SQLGetData(stmt, 2, SQL_C_CHAR, status, sizeof(status), NULL);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        if (string((char*)status) == "Inactive") {
            cout << "Account is inactive.\n";
            return false;
        }

        currentUserRole = (char*)role;
        cout << "Login successful. Role: " << currentUserRole << "\n";
        return true;
    } else {
        cout << "Invalid credentials.\n";
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }
}
// ===== Book Management =====
void addBook() {
    if (currentUserRole != "admin") {
        cout << "Access denied. Only admin can perform this action.\n";
        return;
    }

    string title, author, genre, publisher, isbn, edition, rack, language;
    int year;
    double price;

    cout << "Title: "; getline(cin, title);
    cout << "Author: "; getline(cin, author);
    cout << "Genre: "; getline(cin, genre);
    cout << "Publisher: "; getline(cin, publisher);
    cout << "ISBN (unique): "; getline(cin, isbn);
    if (exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "ISBN already exists.\n"; return;
    }
    cout << "Edition: "; getline(cin, edition);
    cout << "Published Year: "; cin >> year; cin.ignore();
    cout << "Price: "; cin >> price; cin.ignore();
    cout << "Rack Location: "; getline(cin, rack);
    cout << "Language: "; getline(cin, language);

    string sql = "INSERT INTO Books (Title, Author, Genre, Publisher, ISBN, Edition, PublishedYear, Price, RackLocation, Language, Availability) "
                 "VALUES (N'" + title + "', N'" + author + "', N'" + genre + "', N'" + publisher + "', '" + isbn + "', N'" + edition + "', " +
                 to_string(year) + ", " + to_string(price) + ", N'" + rack + "', N'" + language + "', 1)";

    cout << (execSQL(sql) ? "Book added successfully.\n" : "Failed to add book.\n");
}

void updateBook() {
    if (currentUserRole != "admin") {
        cout << "Access denied. Only admin can perform this action.\n";
        return;
    }

    string isbn;
    cout << "Enter ISBN to update: "; getline(cin, isbn);
    if (!exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "Book not found.\n"; return;
    }

    string title, author, genre, publisher, edition, rack, language;
    int year = 0;
    double price = 0;

    cout << "New Title (leave blank to skip): "; getline(cin, title);
    cout << "New Author (leave blank to skip): "; getline(cin, author);
    cout << "New Genre (leave blank to skip): "; getline(cin, genre);
    cout << "New Publisher (leave blank to skip): "; getline(cin, publisher);
    cout << "New Edition (leave blank to skip): "; getline(cin, edition);
    cout << "New Published Year (0 to skip): "; cin >> year; cin.ignore();
    cout << "New Price (0 to skip): "; cin >> price; cin.ignore();
    cout << "New Rack Location (leave blank to skip): "; getline(cin, rack);
    cout << "New Language (leave blank to skip): "; getline(cin, language);

    string sql = "UPDATE Books SET ";
    bool first = true;

    if (!title.empty()) { sql += "Title=N'" + title + "'"; first = false; }
    if (!author.empty()) { sql += (first ? "" : ", ") + string("Author=N'") + author + "'"; first = false; }
    if (!genre.empty()) { sql += (first ? "" : ", ") + string("Genre=N'") + genre + "'"; first = false; }
    if (!publisher.empty()) { sql += (first ? "" : ", ") + string("Publisher=N'") + publisher + "'"; first = false; }
    if (!edition.empty()) { sql += (first ? "" : ", ") + string("Edition=N'") + edition + "'"; first = false; }
    if (year != 0) { sql += (first ? "" : ", ") + string("PublishedYear=") + to_string(year); first = false; }
    if (price != 0) { sql += (first ? "" : ", ") + string("Price=") + to_string(price); first = false; }
    if (!rack.empty()) { sql += (first ? "" : ", ") + string("RackLocation=N'") + rack + "'"; first = false; }
    if (!language.empty()) { sql += (first ? "" : ", ") + string("Language=N'") + language + "'"; }

    sql += " WHERE ISBN='" + isbn + "'";

    cout << (execSQL(sql) ? "Book updated successfully.\n" : "Update failed.\n");
}
void deleteBook() {
    if (currentUserRole != "admin") {
        cout << "Access denied. Only admin can perform this action.\n";
        return;
    }

    string isbn;
    cout << "Enter ISBN to delete: "; getline(cin, isbn);
    if (!exists("SELECT * FROM Books WHERE ISBN='" + isbn + "'")) {
        cout << "Book not found.\n"; return;
    }
    if (exists("SELECT * FROM Transactions WHERE BookID=(SELECT BookID FROM Books WHERE ISBN='" + isbn + "') AND ReturnDate IS NULL")) {
        cout << "Book currently issued, cannot delete.\n"; return;
    }
    cout << (execSQL("DELETE FROM Books WHERE ISBN='" + isbn + "'") ? "Book deleted successfully.\n" : "Delete failed.\n");
}
void viewBooks() {
    int pageSize = 5, page = 0;
    string choice;
    do {
        int offset = page * pageSize;
        string sql = "SELECT BookID, Title, Author, ISBN, Availability FROM books ORDER BY Title OFFSET " +
                     to_string(offset) + " ROWS FETCH NEXT " + to_string(pageSize) + " ROWS ONLY";

        SQLHSTMT stmt;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
            cout << "\nPage " << page + 1 << "\nID\tTitle\tAuthor\tISBN\tAvailability\n";
            while (SQLFetch(stmt) == SQL_SUCCESS) {
                int id, avail;
                char title[255], author[200], isbn[20];
                SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
                SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
                SQLGetData(stmt, 3, SQL_C_CHAR, author, sizeof(author), NULL);
                SQLGetData(stmt, 4, SQL_C_CHAR, isbn, sizeof(isbn), NULL);
                SQLGetData(stmt, 5, SQL_C_LONG, &avail, 0, NULL);
                cout << id << "\t" << title << "\t" << author << "\t" << isbn << "\t" << (avail ? "Yes" : "No") << "\n";
            }
        }
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);

        cout << "[N]ext, [P]revious, [Q]uit: ";
        getline(cin, choice);
        if (choice == "N" || choice == "n") page++;
        else if ((choice == "P" || choice == "p") && page > 0) page--;
    } while (choice != "Q" && choice != "q");
}

void searchBooks() {
    cout << "Enter keyword to search in title: ";
    string keyword; getline(cin, keyword);
    string sql = "SELECT BookID, Title, Author, ISBN FROM books WHERE Title LIKE N'%" + keyword + "%'";

    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    if (SQLExecDirect(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS) == SQL_SUCCESS) {
        cout << "ID\tTitle\tAuthor\tISBN\n";
        while (SQLFetch(stmt) == SQL_SUCCESS) {
            int id;
            char title[255], author[200], isbn[20];
            SQLGetData(stmt, 1, SQL_C_SLONG, &id, 0, NULL);
            SQLGetData(stmt, 2, SQL_C_CHAR, title, sizeof(title), NULL);
            SQLGetData(stmt, 3, SQL_C_CHAR, author, sizeof(author), NULL);
            SQLGetData(stmt, 4, SQL_C_CHAR, isbn, sizeof(isbn), NULL);
            cout << id << "\t" << title << "\t" << author << "\t" << isbn << "\n";
        }
    }
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}
// ===== Member Management =====
void addMember() {
    if (currentUserRole != "admin") { cout << "Access denied.\n"; return; }

    string name, email, phone, address, membership;
    cout << "Name: "; getline(cin, name);
    cout << "Email: "; getline(cin, email);
    if (exists("SELECT * FROM members WHERE email='" + email + "'")) { cout << "Email exists.\n"; return; }
    cout << "Phone: "; getline(cin, phone);
    cout << "Address: "; getline(cin, address);
    cout << "Membership Type: "; getline(cin, membership);

    string sql = "INSERT INTO members (name,email,phone,address,membershiptype,joindate,status) "
                 "VALUES ('" + name + "','" + email + "','" + phone + "','" + address + "','" + membership + "',GETDATE(),'Active')";
    cout << (execSQL(sql) ? "Member added.\n" : "Failed to add member.\n");
}

void updateMember() {
    if (currentUserRole != "admin") {
        cout << "Access denied. Only admin can perform this action.\n";
        return;
    }

    string email;
    cout << "Enter email of member to update: "; getline(cin, email);
    if (!exists("SELECT * FROM members WHERE email='" + email + "'")) {
        cout << "Member not found.\n"; 
        return;
    }

    string phone, address, membership;
    cout << "New phone (leave blank to skip): "; getline(cin, phone);
    cout << "New address (leave blank to skip): "; getline(cin, address);
    cout << "New Membership Type (Regular/Premium, leave blank to skip): "; getline(cin, membership);

    string sql = "UPDATE members SET ";
    bool first = true;

    if (!phone.empty()) { sql += (first ? "" : ", ") + string("Phone=N'") + phone + "'"; first=false; }
    if (!address.empty()) { sql += (first ? "" : ", ") + string("Address=N'") + address + "'"; first=false; }
    if (!membership.empty()) { sql += (first ? "" : ", ") + string("MembershipType=N'") + membership + "'"; }

    sql += " WHERE Email='" + email + "'";
    cout << (execSQL(sql) ? "Member updated successfully.\n" : "Failed to update member.\n");
}