#!/bin/bash
# filepath: /home/ebit/exp/002-400/traffic-light-web/deploy_nginx.sh

set -e

# 1. 构建前端
echo "正在构建前端..."
npm install
npm run build

# 2. 安装 Nginx（如果未安装）
if ! command -v nginx >/dev/null 2>&1; then
    echo "正在安装 Nginx..."
    sudo apt update
    sudo apt install -y nginx
fi

# 3. 配置 Nginx
NGINX_CONF="/etc/nginx/sites-available/traffic-light-web"
DIST_PATH="/home/admin/002-400/traffic-light-web/dist"

sudo tee $NGINX_CONF > /dev/null <<EOF
server {
    listen 80;
    server_name 39.107.106.220;

    root $DIST_PATH;
    index index.html index.htm;

    location / {
        try_files \$uri \$uri/ /index.html;
    }
}
EOF

# 更改 dist 目录所有权和权限
echo "正在更改 $DIST_PATH 的所有权和权限..."
sudo chown -R www-data:www-data $DIST_PATH
sudo find $DIST_PATH -type d -exec chmod 755 {} \; # 目录权限 rwxr-xr-x
sudo find $DIST_PATH -type f -exec chmod 644 {} \; # 文件权限 rw-r--r--

# 4. 启用站点配置
sudo ln -sf $NGINX_CONF /etc/nginx/sites-enabled/traffic-light-web

# 5. 移除默认站点（可选）
sudo rm -f /etc/nginx/sites-enabled/default

# 6. 检查并重启 Nginx
sudo nginx -t
sudo systemctl restart nginx

# 7. 开放 80 端口（如使用 ufw 防火墙）
if command -v ufw >/dev/null 2>&1; then
    sudo ufw allow 80/tcp
fi

echo "部署完成！现在可以通过 http://服务器IP 访问前端页面。"

