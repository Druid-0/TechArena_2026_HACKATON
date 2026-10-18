//! SQL baseline parser. Reads the real warmup dump, never executes the services.
//! The dump contains `INSERT INTO `orders`` and `orders_other` values.
use flate2::read::GzDecoder;
use std::fs::File;
use std::io::Read;
use std::path::Path;

#[derive(Clone, Debug)]
pub struct OrderRow {
    pub id: String,
    pub account: String,
    pub status: i32,
    pub train_id: String,
    pub departure: String,
}

pub struct DbSnapshot {
    pub high: Vec<OrderRow>,
    pub regular: Vec<OrderRow>,
}

// Extract INSERT values while respecting SQL quoted strings and escaped quotes.
fn parse_tuples(raw: &str) -> Vec<Vec<String>> {
    let mut tuples = Vec::new();
    let mut row = Vec::new();
    let mut field = String::new();
    let mut quoted = false;
    let mut active = false;
    let chars: Vec<char> = raw.chars().collect();
    let mut i = 0;
    while i < chars.len() {
        let c = chars[i];
        if c == '\\' && quoted && i + 1 < chars.len() {
            field.push(chars[i + 1]); i += 2; continue;
        }
        if c == '\'' {
            if quoted && i + 1 < chars.len() && chars[i + 1] == '\'' {
                field.push('\''); i += 2; continue;
            }
            quoted = !quoted; i += 1; continue;
        }
        if !quoted {
            match c {
                '(' if !active => {active=true; row.clear(); field.clear();},
                ',' if active => {row.push(field.trim().to_owned()); field.clear();},
                ')' if active => {
                    row.push(field.trim().to_owned()); field.clear();
                    tuples.push(std::mem::take(&mut row)); active=false;
                },
                ';' => break,
                _ if active => field.push(c),
                _ => {},
            }
        } else if active {field.push(c);}
        i += 1;
    }
    tuples
}

fn order_rows(sql: &str, name: &str) -> Result<Vec<OrderRow>, String> {
    let prefix = format!("INSERT INTO `{name}` VALUES ");
    let mut output = Vec::new();
    for line in sql.lines().filter_map(|s| s.trim_start().strip_prefix(&prefix)) {
        for row in parse_tuples(line) {
            if row.len() < 15 { return Err(format!("Bad {name} row: {} columns", row.len())); }
            output.push(OrderRow {
                id: row[0].clone(),
                account: row[1].clone(),
                status: row[11].parse::<i32>().map_err(|e|e.to_string())?,
                train_id: row[13].clone(),
                departure: row[14].clone(),
            });
        }
    }
    if output.is_empty() {return Err(format!("No rows found in SQL table {name}"));}
    Ok(output)
}
impl DbSnapshot {
    pub fn load(path: &Path) -> Result<Self, Box<dyn std::error::Error>> {
        let mut bytes = Vec::new();
        GzDecoder::new(File::open(path)?).read_to_end(&mut bytes)?;
        // Some unrelated seed values contain legacy non-UTF-8 bytes.
        let sql = String::from_utf8_lossy(&bytes);
        Ok(Self {
            high: order_rows(&sql, "orders")?,
            regular: order_rows(&sql, "orders_other")?,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn quoted_comma_and_escaped_quote() {
        let got=parse_tuples("('a,b','c''d',2),('e','f',3);");
        assert_eq!(got, vec![vec!["a,b", "c'd", "2"], vec!["e", "f", "3"]]);
    }
}
