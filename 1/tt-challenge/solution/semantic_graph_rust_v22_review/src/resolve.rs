//! Recover project calls omitted by the skeleton's shallow receiver typing.
//! Generated Lombok getters are used only for type inference, never emitted.
use serde_json::Value;
use std::collections::HashMap;
use std::path::Path;
use crate::{str_prop, Index, add_adj};

fn compact(s: &str) -> String { s.chars().filter(|c| !c.is_whitespace()).collect() }

struct Types {
    variables: HashMap<(String,String),String>,
    fields: HashMap<(String,String),String>,
    returns: HashMap<(String,String),String>,
    names: HashMap<String,Vec<(String,String)>>,
}
impl Types {
    fn receiver(&self, expr: &str, method: &str, module: &str, depth: usize) -> Option<String> {
        if depth > 12 { return None; }
        let expr = compact(expr);
        if let Some(t) = self.variables.get(&(method.to_owned(),expr.clone())) { return Some(t.clone()); }
        if let Some(stem) = expr.strip_suffix("()") {
            let (base, name) = stem.rsplit_once('.')?;
            let typ = self.receiver(base,method,module,depth+1)?;
            if let Some(t) = self.returns.get(&(typ.clone(),name.to_owned())) { return Some(t.clone()); }
            // @Data's generated accessors still supply the static result type.
            let property = name.strip_prefix("get").or_else(||name.strip_prefix("is"))?;
            let mut chars = property.chars();
            let field = format!("{}{}", chars.next()?.to_lowercase(),chars.as_str());
            return self.fields.get(&(typ,field)).cloned();
        }
        if let Some((base,field)) = expr.rsplit_once('.') {
            let typ = self.receiver(base,method,module,depth+1)?;
            return self.fields.get(&(typ,field.to_owned())).cloned();
        }
        let matches = self.names.get(&expr)?;
        matches.iter().find(|(m,_)|m==module)
            .or_else(||matches.iter().find(|(m,_)|m=="ts-common"))
            .map(|(_,t)|t.clone())
    }
}

pub fn recover(ix: &mut Index, skeleton: &Value, root: &Path) -> Result<usize,String> {
    let nodes = skeleton["nodes"].as_object().ok_or("missing nodes")?;
    let mut types = Types { variables:HashMap::new(), fields:HashMap::new(), returns:HashMap::new(), names:HashMap::new() };
    let owner_name = |n:&Value| str_prop(n,"owner").splitn(3,':').nth(2).unwrap_or("").to_owned();
    for n in nodes.values() {
        match str_prop(n,"kind") {
            "type" => {
                let fqn = str_prop(n,"fqn");
                types.names.entry(fqn.rsplit('.').next().unwrap_or(fqn).to_owned()).or_default()
                    .push((str_prop(n,"module").to_owned(),fqn.to_owned()));
            },
            "field" => {types.fields.insert((owner_name(n),str_prop(n,"name").to_owned()),str_prop(n,"type").to_owned());},
            "method" if str_prop(n,"descriptor").is_empty() => {
                types.returns.insert((owner_name(n),str_prop(n,"name").to_owned()),str_prop(n,"return").to_owned());
            },
            "call_site" if !str_prop(n,"receiver_type").is_empty() => {
                types.variables.insert((str_prop(n,"method").to_owned(),compact(str_prop(n,"receiver"))),str_prop(n,"receiver_type").to_owned());
            },
            _ => {}
        }
    }
    let mut sources = HashMap::new();
    let mut count=0;
    for n in nodes.values().filter(|n| str_prop(n,"kind")=="call_site" && str_prop(n,"receiver_type").is_empty()) {
        let method = str_prop(n,"method");
        let module = str_prop(n,"module");
        let Some(typ) = types.receiver(str_prop(n,"receiver"),method,module,0) else {continue};
        let name = str_prop(n,"callee");
        let argc = n["arg_count"].as_u64().unwrap_or(999) as usize;
        let candidates: Vec<_> = nodes.iter().filter(|(_,v)| str_prop(v,"kind")=="method"
            && owner_name(v)==typ && str_prop(v,"name")==name
            && v["params_as_written"].as_array().map(|p|p.len())==Some(argc)).collect();
        if candidates.len()!=1 {continue;}
        // Confirm the call site against the supplied source, not a predicted answer.
        let file=str_prop(n,"file");
        if !sources.contains_key(file) {
            sources.insert(file.to_owned(),std::fs::read_to_string(root.join(file)).map_err(|e|format!("{file}: {e}"))?);
        }
        let line = n["line"].as_u64().unwrap_or(0) as usize;
        if !sources[file].lines().nth(line.saturating_sub(1)).unwrap_or("").contains(name) {continue;}
        let target=candidates[0].0;
        if !ix.calls.get(method).map(|v|v.contains(target)).unwrap_or(false) {
            add_adj(&mut ix.calls,method,target); count+=1;
        }
    }
    Ok(count)
}
