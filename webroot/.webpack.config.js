const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const fs = require('fs');

module.exports = {
  entry: {}, // no JS entry because we're using pre-built files
  output: {
    filename: '[name].js', // unused but required by webpack
    path: path.resolve(__dirname, '../build/webroot'), // output folder
    clean: true, // cleans build/webroot before building
  },
  plugins: [
    new HtmlWebpackPlugin({
      templateContent: ({ htmlWebpackPlugin }) => {
        // Read pre-minified JS and CSS
        const js = fs.readFileSync(path.resolve(__dirname, '../build/webroot/app.min.js'), 'utf8');
        const css = fs.readFileSync(path.resolve(__dirname, '../build/webroot/styles.min.css'), 'utf8');

        // Read the base index.html
        const html = fs.readFileSync(path.resolve(__dirname, 'index.html'), 'utf8');

        // Return HTML with inlined JS and CSS
        return `
<!DOCTYPE html>
<link rel="icon" href="data:,">
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Config Editor</title>
  <style>${css}</style>
</head>
<body>
  ${html}
  <script>${js}</script>
</body>
</html>
        `;
      },
      filename: 'index.html', // output filename
    }),
  ],
  mode: 'production',
};
