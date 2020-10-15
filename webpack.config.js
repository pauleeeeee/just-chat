const webpack = require('webpack')
const path = require('path')
const os = require('os')

function relative(p) {
  return path.relative(__dirname, path.resolve(os.homedir(), p))
}

module.exports = {
  devtool: 'source-map',
  entry: [
    'core-js/stable',
    'regenerator-runtime/runtime',
    'whatwg-fetch',
    relative('./Library/Application Support/Pebble SDK/SDKs/current/sdk-core/pebble/common/include/_pkjs_shared_additions.js'),
    './src/pkjs/index.js'
  ],
  output: {
    filename: 'pebble-js-app.js',
    path: path.resolve(__dirname, './build'),
    sourceMapFilename: 'pebble-js-app.js.map',
    devtoolModuleFilenameTemplate: '[resource-path]',
    devtoolFallbackModuleFilenameTemplate: '[resource-path]?[hash]',
  },
  target: 'es5',
  resolve: {
    roots: [
      relative('./Library/Application Support/Pebble SDK/SDKs/current/sdk-core/pebble/common/include')
    ],
    extensions: ['.js', '.json'],
    alias: {
      'app_package.json': path.resolve(__dirname, './package.json'),
      'message_keys': path.resolve(__dirname, './build/js/message_keys.json')
    }
  },

  module: {
    rules: [
      {
        test: /\.js$/,
        exclude: /node_modules/,
        use: {
          loader: 'babel-loader',
          options: {
            presets: [ '@babel/preset-env' ],
          }
        }
      }
    ]
  }
}
