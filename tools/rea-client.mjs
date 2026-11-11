import { Client } from './rea/node_modules/@modelcontextprotocol/client/dist/index.mjs';
import { StdioClientTransport } from './rea/node_modules/@modelcontextprotocol/client/dist/stdio.mjs';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
const client = new Client({ name: 'obsbot-rebuild', version: '1.0.0' });
const transport = new StdioClientTransport({command: process.execPath, args: [fileURLToPath(new URL('./rea/node_modules/rea-agents/scripts/rea.mjs', import.meta.url)), '--mcp'], env: {...process.env, REA_MCP_OUTPUT_SCHEMA_PROFILE:'off'}, stderr:'inherit'});
try {
 await client.connect(transport);
 const list = await client.listTools();
 const name = process.argv[2];
 if (!name) console.log(JSON.stringify(list.tools.filter(t => /desktop|pe.import|signature|binary.layout/.test(t.name)), null, 2));
 else {
 const result = await client.callTool({name, arguments: JSON.parse(process.argv[3] || '{}')});
 if(process.argv[4]) fs.writeFileSync(process.argv[4], JSON.stringify(result,null,2));
 console.log(JSON.stringify(process.argv[4] ? {saved:process.argv[4],isError:result.isError,summary:result.structuredContent?.normalized_result?.coverage} : result,null,2));
 }
} finally { await client.close(); }
