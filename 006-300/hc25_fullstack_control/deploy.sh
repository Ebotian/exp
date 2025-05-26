#!/bin/bash

# --- 配置变量 ---
REMOTE_USER="admin"
REMOTE_HOST="39.107.106.220"
SSH_KEY="~/.ssh/id_rsa" # 你的 SSH 私钥路径
SSH_CMD="ssh -i ${SSH_KEY} ${REMOTE_USER}@${REMOTE_HOST}"
SCP_CMD="scp -i ${SSH_KEY}"

LOCAL_FRONTEND_DIR="./frontend"
LOCAL_BACKEND_DIR="./backend"

REMOTE_BASE_DIR="/var/www/hc25_control" # 服务器上部署应用的基础目录
REMOTE_FRONTEND_DIR="${REMOTE_BASE_DIR}/frontend_dist"
REMOTE_BACKEND_DIR="${REMOTE_BASE_DIR}/backend"
REMOTE_VENV_DIR="${REMOTE_BACKEND_DIR}/venv"

NGINX_CONF_NAME="hc25_frontend" # Nginx 配置文件名
NGINX_SITES_AVAILABLE="/etc/nginx/sites-available"
NGINX_SITES_ENABLED="/etc/nginx/sites-enabled"

BACKEND_PORT="5001" # 后端 Flask 应用监听的端口
FRONTEND_PORT="100" # Nginx 为前端监听的端口
# TCP_MODULE_PORT="9999" # 这个由 backend/app.py 控制

# --- 函数定义 ---
echo_color() {
    COLOR=$1
    TEXT=$2
    case $COLOR in
        "green") echo -e "\033[32m${TEXT}\033[0m" ;;
        "red") echo -e "\033[31m${TEXT}\033[0m" ;;
        "yellow") echo -e "\033[33m${TEXT}\033[0m" ;;
        *) echo "${TEXT}" ;;
    esac
}

# --- 脚本开始 ---
echo_color "yellow" "开始部署 HC25 远程控制应用..."

# 1. 本地构建前端
echo_color "yellow" "1. 正在本地构建前端应用..."
cd "${LOCAL_FRONTEND_DIR}" || { echo_color "red" "错误: 无法进入前端目录 ${LOCAL_FRONTEND_DIR}"; exit 1; }
npm install || { echo_color "red" "错误: npm install 失败"; exit 1; }
npm run build || { echo_color "red" "错误: npm run build 失败"; exit 1; }
cd .. # 返回项目根目录
echo_color "green" "前端构建完成。"

# 2. 在服务器上创建目录结构
echo_color "yellow" "2. 正在服务器上创建目录结构..."
${SSH_CMD} "sudo mkdir -p ${REMOTE_FRONTEND_DIR} && \
            sudo mkdir -p ${REMOTE_BACKEND_DIR} && \
            sudo chown -R ${REMOTE_USER}:${REMOTE_USER} ${REMOTE_BASE_DIR}" || \
            { echo_color "red" "错误: 无法在服务器上创建目录"; exit 1; }
echo_color "green" "服务器目录结构准备就绪。"

# 3. 上传文件到服务器
echo_color "yellow" "3. 正在上传文件到服务器..."
# 上传前端构建文件 (dist 目录下的所有内容)
${SCP_CMD} -r "${LOCAL_FRONTEND_DIR}/dist/." "${REMOTE_USER}@${REMOTE_HOST}:${REMOTE_FRONTEND_DIR}/" || \
    { echo_color "red" "错误: 上传前端文件失败"; exit 1; }
# 上传后端文件 (除了 venv)
${SCP_CMD} -r "${LOCAL_BACKEND_DIR}/app.py" "${REMOTE_USER}@${REMOTE_HOST}:${REMOTE_BACKEND_DIR}/" || \
    { echo_color "red" "错误: 上传后端 app.py 失败"; exit 1; }
# 如果你有 requirements.txt，也应该上传
# ${SCP_CMD} "${LOCAL_BACKEND_DIR}/requirements.txt" "${REMOTE_USER}@${REMOTE_HOST}:${REMOTE_BACKEND_DIR}/"

echo_color "green" "文件上传完成。"

# 4. 在服务器上设置后端环境并启动后端
echo_color "yellow" "4. 正在服务器上设置并启动后端应用..."
${SSH_CMD} << EOF
    echo "--- 进入后端目录 ${REMOTE_BACKEND_DIR} ---"
    cd "${REMOTE_BACKEND_DIR}" || exit 1

    echo "--- 检查并创建 Python 虚拟环境 ---"
    if [ ! -d "${REMOTE_VENV_DIR}" ]; then
        python3 -m venv venv || exit 1
        echo "虚拟环境已创建。"
    fi

    echo "--- 激活虚拟环境并安装依赖 ---"
    source "${REMOTE_VENV_DIR}/bin/activate" || exit 1
    pip install Flask flask-cors || exit 1
    # 如果有 requirements.txt: pip install -r requirements.txt
    deactivate

    echo "--- 停止可能已在运行的旧后端实例 (基于端口 ${BACKEND_PORT}) ---"
    # 注意: pkill 可能过于粗暴，生产环境建议用 systemd 或 supervisor
    sudo pkill -f "python app.py" # 这会杀死所有名为 python app.py 的进程
    # 更精确的方式是找到监听特定端口的进程并杀死，但这更复杂
    # LSOF_PID=\$(sudo lsof -t -i:${BACKEND_PORT})
    # if [ -n "\$LSOF_PID" ]; then
    #    sudo kill -9 \$LSOF_PID
    # fi
    sleep 2 # 等待进程关闭

    echo "--- 以后台方式启动后端应用 (监听端口 ${BACKEND_PORT}) ---"
    source "${REMOTE_VENV_DIR}/bin/activate" || exit 1
    nohup python app.py > backend.log 2>&1 &
    # 确保 app.py 中的 Flask app.run host 设置为 '0.0.0.0' 且 port 为 ${BACKEND_PORT}
    # 并且 TCP 服务器监听 9999 端口
    deactivate
    echo "后端应用应该已在后台启动。检查 backend.log 获取日志。"
EOF
if [ $? -ne 0 ]; then echo_color "red" "错误: 服务器后端设置或启动失败"; exit 1; fi
echo_color "green" "服务器后端设置并尝试启动。"

# 5. 在服务器上配置 Nginx
echo_color "yellow" "5. 正在服务器上配置 Nginx..."
# 创建 Nginx 配置文件内容
NGINX_CONFIG_CONTENT="server {
    listen ${FRONTEND_PORT};
    server_name ${REMOTE_HOST}; # 或者你的域名

    root ${REMOTE_FRONTEND_DIR};
    index index.html index.htm;

    location / {
        try_files \$uri \$uri/ /index.html;
    }

    location /api/ {
        proxy_pass http://localhost:${BACKEND_PORT}/api/;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
    }

    access_log /var/log/nginx/${NGINX_CONF_NAME}.access.log;
    error_log /var/log/nginx/${NGINX_CONF_NAME}.error.log;
}
"
# 将 Nginx 配置写入服务器上的文件
${SSH_CMD} "echo '${NGINX_CONFIG_CONTENT}' | sudo tee ${NGINX_SITES_AVAILABLE}/${NGINX_CONF_NAME} > /dev/null" || \
    { echo_color "red" "错误: 无法在服务器上写入 Nginx 配置文件"; exit 1; }

# 创建软链接以启用站点
${SSH_CMD} "sudo ln -sf ${NGINX_SITES_AVAILABLE}/${NGINX_CONF_NAME} ${NGINX_SITES_ENABLED}/${NGINX_CONF_NAME}" || \
    { echo_color "red" "错误: 无法创建 Nginx 站点软链接"; exit 1; }

# 测试 Nginx 配置并重新加载
${SSH_CMD} "sudo nginx -t && sudo systemctl reload nginx"
if [ $? -ne 0 ]; then
    echo_color "red" "错误: Nginx 配置测试失败或无法重新加载 Nginx。"
    echo_color "yellow" "请登录服务器检查 Nginx 错误: sudo nginx -t 和 journalctl -xeu nginx.service"
    exit 1
fi
echo_color "green" "Nginx 配置完成并已重新加载。"

echo_color "green" "部署完成！"
echo_color "yellow" "请检查以下事项："
echo_color "yellow" "- 确保你的物联网模块已配置为连接到 ${REMOTE_HOST} 的 9999 TCP 端口。"
echo_color "yellow" "- 在浏览器中访问 http://${REMOTE_HOST}:${FRONTEND_PORT}"
echo_color "yellow" "- 检查服务器上的后端日志: ${SSH_CMD} \"tail -f ${REMOTE_BACKEND_DIR}/backend.log\""
echo_color "yellow" "- 检查服务器上的 Nginx 日志: /var/log/nginx/${NGINX_CONF_NAME}.access.log 和 .error.log"
echo_color "yellow" "- 确保服务器防火墙允许 TCP 端口 ${FRONTEND_PORT}, ${BACKEND_PORT}, 和 9999 的入站连接。"
