const fs = require('fs')

const textDecoder = new TextDecoder()

let wasm

const env = {
  print (str, len) {
    const buf = wasm.instance.exports.memory.buffer.slice(str, str + len)
    const text = textDecoder.decode(buf)
    console.log(text)
  }
}

;(async () => {
  try {
    let [, , func, ...args] = process.argv

    if (!func) func = 'main'

    const buf = fs.readFileSync('out.wasm')

    wasm = await WebAssembly.instantiate(buf, { env })

    if (!wasm.instance.exports[func]) {
      console.log()
      throw `${func} is not a function exported from the module`
    }

    const result = wasm.instance.exports[func](...args)
    // undefined is returned if the function has the u0 return type
    if (result !== undefined) console.log(result)
    process.exit(0)
  } catch (e) {
    console.log(e)
    process.exit(1)
  }
})()
